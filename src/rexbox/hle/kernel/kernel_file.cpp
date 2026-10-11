// File system services over the VFS (kernel_vfs.h). I/O is synchronous: a
// request completes before the call returns, its event (if any) is signalled
// and its IO_STATUS_BLOCK filled. APC completion routines are not delivered.

#include "kernel_internal.h"
#include "kernel_vfs.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <system_error>

namespace rexbox::hle::kernel
{

namespace
{

using runtime::HleCall;
namespace fs = std::filesystem;

// CreateDisposition
constexpr uint32_t kFileSupersede = 0;
constexpr uint32_t kFileOpen = 1;
constexpr uint32_t kFileCreate = 2;
constexpr uint32_t kFileOpenIf = 3;
constexpr uint32_t kFileOverwrite = 4;
constexpr uint32_t kFileOverwriteIf = 5;
// CreateOptions
constexpr uint32_t kFileDirectoryFile = 0x01;
constexpr uint32_t kFileNonDirectoryFile = 0x40;
// IO_STATUS_BLOCK.Information for creates
constexpr uint32_t kFileSuperseded = 0;
constexpr uint32_t kFileOpened = 1;
constexpr uint32_t kFileCreated = 2;
constexpr uint32_t kFileOverwritten = 3;
// DesiredAccess bits that mean writing
constexpr uint32_t kWriteAccess = 0x40000000 | 0x00000002 | 0x00000004 | 0x00000100 | 0x00010000;
// File attributes
constexpr uint32_t kAttributeReadOnly = 0x01;
constexpr uint32_t kAttributeDirectory = 0x10;
constexpr uint32_t kAttributeNormal = 0x80;
// LARGE_INTEGER ByteOffset meaning "use the current position"
constexpr uint32_t kUseFilePointerHigh = 0xFFFFFFFF;
constexpr uint32_t kUseFilePointerLow = 0xFFFFFFFE;

std::string ReadAnsiString(GuestAddress string)
{
    if (string == 0)
    {
        return {};
    }
    runtime::GuestMemory& memory = Memory();
    const uint16_t length = memory.Read16(string);
    const GuestAddress buffer = memory.Read32(string + 4);
    std::string text(length, '\0');
    for (uint16_t i = 0; i < length; ++i)
    {
        text[i] = static_cast<char>(memory.Read8(buffer + i));
    }
    return text;
}

// OBJECT_ATTRIBUTES: RootDirectory (+0), ObjectName (+4, PANSI_STRING), Attributes (+8).
std::string ObjectName(GuestAddress attributes)
{
    runtime::GuestMemory& memory = Memory();
    std::string name = ReadAnsiString(memory.Read32(attributes + 4));
    const uint32_t root = memory.Read32(attributes + 0);
    if (root != 0)
    {
        if (const std::shared_ptr<KernelObject> parent = LookupHandle(root))
        {
            name = parent->xbox_path + (name.empty() || name[0] == '\\' ? "" : "\\") + name;
        }
    }
    return name;
}

void WriteIoStatus(GuestAddress io_status, uint32_t status, uint32_t information)
{
    if (io_status != 0)
    {
        Memory().Write32(io_status + 0, status);
        Memory().Write32(io_status + 4, information);
    }
}

void SignalEventIfAny(uint32_t event_handle)
{
    if (event_handle == 0)
    {
        return;
    }
    const std::shared_ptr<KernelObject> event = LookupHandle(event_handle);
    if (event != nullptr && event->object != 0)
    {
        std::lock_guard<std::mutex> lock(State().dispatcher);
        Memory().Write32(event->object + layout::kHeaderSignalState, 1);
        State().signalled.notify_all();
    }
}

uint64_t ToNtTime(fs::file_time_type time)
{
    // file_time_type's epoch is unspecified; anchor it through "now".
    const auto now_file = fs::file_time_type::clock::now();
    const auto delta = std::chrono::duration_cast<std::chrono::nanoseconds>(now_file - time).count();
    return SystemTimeNt() - static_cast<uint64_t>(delta / 100);
}

uint32_t AttributesOf(const fs::path& path, bool read_only)
{
    std::error_code ec;
    if (fs::is_directory(path, ec))
    {
        return kAttributeDirectory | (read_only ? kAttributeReadOnly : 0);
    }
    return read_only ? kAttributeReadOnly : kAttributeNormal;
}

uint32_t OpenOrCreateUnlogged(GuestAddress handle_out, uint32_t access, GuestAddress attributes,
                              GuestAddress io_status, uint32_t disposition, uint32_t options);

// Opens or creates, logging failures once per name -- and every request when
// REXBOX_TRACE_FILES is set.
uint32_t OpenOrCreate(GuestAddress handle_out, uint32_t access, GuestAddress attributes, GuestAddress io_status,
                      uint32_t disposition, uint32_t options)
{
    static const bool trace_all = std::getenv("REXBOX_TRACE_FILES") != nullptr;
    const uint32_t status = OpenOrCreateUnlogged(handle_out, access, attributes, io_status, disposition, options);
    const std::string name = ObjectName(attributes);
    if (trace_all)
    {
        std::fprintf(stderr, "[kernel] open %s disposition %u -> %08X\n", name.c_str(), disposition, status);
    }
    else if (status != kStatusSuccess)
    {
        char code[16];
        std::snprintf(code, sizeof(code), "%08X", status);
        TraceOnce("NtCreateFile/NtOpenFile", name + " -> " + code);
    }
    return status;
}

uint32_t OpenOrCreateUnlogged(GuestAddress handle_out, uint32_t access, GuestAddress attributes,
                              GuestAddress io_status, uint32_t disposition, uint32_t options)
{
    const std::string name = ObjectName(attributes);
    const std::optional<VfsPath> resolved = VfsResolve(name);
    if (!resolved)
    {
        TraceOnce("NtCreateFile", "no device for " + name);
        WriteIoStatus(io_status, kStatusObjectPathNotFound, 0);
        return kStatusObjectPathNotFound;
    }
    if (resolved->raw_device)
    {
        // The whole disk: no files, only sector I/O and IOCTLs.
        auto device = std::make_shared<KernelObject>();
        device->kind = KernelObject::Kind::kDevice;
        device->xbox_path = resolved->xbox_path;
        uint32_t handle = 0;
        {
            std::lock_guard<std::mutex> lock(State().dispatcher);
            handle = InsertHandleLocked(device);
        }
        Memory().Write32(handle_out, handle);
        WriteIoStatus(io_status, kStatusSuccess, kFileOpened);
        TraceOnce("NtCreateFile", "raw device " + resolved->xbox_path + " options " + std::to_string(options));
        return kStatusSuccess;
    }
    std::error_code ec;
    const fs::path& host = resolved->host_path;
    const bool exists = fs::exists(host, ec);
    const bool is_directory = exists && fs::is_directory(host, ec);
    const bool wants_write = (access & kWriteAccess) != 0 || disposition != kFileOpen;

    auto object = std::make_shared<KernelObject>();
    object->xbox_path = resolved->xbox_path;
    object->host_path = host;
    object->read_only = resolved->read_only;
    object->volume = resolved->volume;
    uint32_t information = kFileOpened;

    if ((options & kFileDirectoryFile) != 0 || is_directory)
    {
        if ((options & kFileNonDirectoryFile) != 0 && is_directory)
        {
            WriteIoStatus(io_status, kStatusAccessDenied, 0);
            return kStatusAccessDenied;
        }
        if (!exists)
        {
            if (disposition == kFileOpen || resolved->read_only || !fs::exists(host.parent_path(), ec))
            {
                const uint32_t status = fs::exists(host.parent_path(), ec) ? kStatusObjectNameNotFound
                                                                            : kStatusObjectPathNotFound;
                WriteIoStatus(io_status, status, 0);
                return status;
            }
            fs::create_directory(host, ec);
            information = kFileCreated;
        }
        object->kind = KernelObject::Kind::kDirectory;
    }
    else
    {
        if (!fs::exists(host.parent_path(), ec))
        {
            WriteIoStatus(io_status, kStatusObjectPathNotFound, 0);
            return kStatusObjectPathNotFound;
        }
        if (exists && disposition == kFileCreate)
        {
            WriteIoStatus(io_status, kStatusObjectNameCollision, 0);
            return kStatusObjectNameCollision;
        }
        if (!exists && (disposition == kFileOpen || disposition == kFileOverwrite))
        {
            WriteIoStatus(io_status, kStatusObjectNameNotFound, 0);
            return kStatusObjectNameNotFound;
        }
        if (resolved->read_only && wants_write && disposition != kFileOpen)
        {
            WriteIoStatus(io_status, kStatusAccessDenied, 0);
            return kStatusAccessDenied;
        }
        std::ios::openmode mode = std::ios::binary | std::ios::in;
        const bool truncate = disposition == kFileSupersede || disposition == kFileOverwrite ||
                              disposition == kFileOverwriteIf || !exists;
        if (!resolved->read_only)
        {
            mode |= std::ios::out;
            if (truncate)
            {
                mode |= std::ios::trunc;
            }
        }
        auto stream = std::make_shared<std::fstream>(host, mode);
        if (!stream->is_open() && !resolved->read_only && !truncate)
        {
            // A read-only host file on a writable device: fall back to reading.
            stream = std::make_shared<std::fstream>(host, std::ios::binary | std::ios::in);
        }
        if (!stream->is_open())
        {
            WriteIoStatus(io_status, kStatusAccessDenied, 0);
            return kStatusAccessDenied;
        }
        object->stream = stream;
        object->kind = KernelObject::Kind::kFile;
        if (!exists)
        {
            information = kFileCreated;
        }
        else if (disposition == kFileSupersede)
        {
            information = kFileSuperseded;
        }
        else if (truncate)
        {
            information = kFileOverwritten;
        }
    }

    uint32_t handle = 0;
    {
        std::lock_guard<std::mutex> lock(State().dispatcher);
        handle = InsertHandleLocked(object);
    }
    Memory().Write32(handle_out, handle);
    WriteIoStatus(io_status, kStatusSuccess, information);
    return kStatusSuccess;
}

// NTSTATUS NtCreateFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes,
//     PIO_STATUS_BLOCK IoStatusBlock, PLARGE_INTEGER AllocationSize, ULONG FileAttributes,
//     ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions)
void NtCreateFile()
{
    HleCall call;
    const uint32_t status = OpenOrCreate(call.Arg(0), call.Arg(1), call.Arg(2), call.Arg(3), call.Arg(7), call.Arg(8));
    call.Return(status, 9);
}

// NTSTATUS NtOpenFile(PHANDLE FileHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes,
//     PIO_STATUS_BLOCK IoStatusBlock, ULONG ShareAccess, ULONG OpenOptions)
void NtOpenFile()
{
    HleCall call;
    const uint32_t status = OpenOrCreate(call.Arg(0), call.Arg(1), call.Arg(2), call.Arg(3), kFileOpen, call.Arg(5));
    call.Return(status, 6);
}

std::shared_ptr<KernelObject> FileFromHandle(uint32_t handle)
{
    std::shared_ptr<KernelObject> object = LookupHandle(handle);
    if (object == nullptr || (object->kind != KernelObject::Kind::kFile && object->kind != KernelObject::Kind::kDirectory &&
                              object->kind != KernelObject::Kind::kDevice))
    {
        return nullptr;
    }
    return object;
}

std::string Hex(uint64_t value)
{
    char text[24];
    std::snprintf(text, sizeof(text), "%llX", static_cast<unsigned long long>(value));
    return text;
}

// Raw device and volume I/O. Files live in host directories, so there are no
// on-disk structures behind this: reads return zeros and writes (a volume
// being formatted, say) are dropped. Every distinct request is traced to learn
// what the title expects there.
uint32_t DeviceTransfer(KernelObject& device, bool write, GuestAddress buffer, uint32_t length, uint64_t offset,
                        GuestAddress io_status)
{
    // Reads are few and their offsets say what is being looked for; writes
    // come by the hundred when a volume is formatted, so one line per device.
    TraceOnce(write ? "NtWriteFile" : "NtReadFile",
              write ? device.xbox_path + " raw writes dropped"
                    : device.xbox_path + " offset 0x" + Hex(offset) + " length 0x" + Hex(length));
    if (!write)
    {
        Memory().Fill(buffer, 0, length);
    }
    device.position = offset + length;
    WriteIoStatus(io_status, kStatusSuccess, length);
    return kStatusSuccess;
}

uint64_t ResolveOffset(KernelObject& file, GuestAddress byte_offset)
{
    if (byte_offset == 0)
    {
        return file.position;
    }
    const uint32_t low = Memory().Read32(byte_offset);
    const uint32_t high = Memory().Read32(byte_offset + 4);
    if (high == kUseFilePointerHigh && low == kUseFilePointerLow)
    {
        return file.position;
    }
    return static_cast<uint64_t>(low) | (static_cast<uint64_t>(high) << 32);
}

// NTSTATUS NtReadFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext,
//     PIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length, PLARGE_INTEGER ByteOffset)
void NtReadFile()
{
    HleCall call;
    const std::shared_ptr<KernelObject> file = FileFromHandle(call.Arg(0));
    const GuestAddress io_status = call.Arg(4);
    const GuestAddress buffer = call.Arg(5);
    const uint32_t length = call.Arg(6);
    if (file != nullptr && (file->kind == KernelObject::Kind::kDevice || file->volume))
    {
        const uint32_t status = DeviceTransfer(*file, false, buffer, length, ResolveOffset(*file, call.Arg(7)), io_status);
        SignalEventIfAny(call.Arg(1));
        call.Return(status, 8);
        return;
    }
    if (file == nullptr || file->stream == nullptr)
    {
        call.Return(kStatusInvalidHandle, 8);
        return;
    }
    if (call.Arg(2) != 0)
    {
        TraceOnce("NtReadFile", "APC completion routine not delivered");
    }
    const uint64_t offset = ResolveOffset(*file, call.Arg(7));
    std::fstream& stream = *file->stream;
    stream.clear();
    stream.seekg(static_cast<std::streamoff>(offset));
    stream.read(reinterpret_cast<char*>(Memory().Translate(buffer)), length);
    const uint32_t read = static_cast<uint32_t>(stream.gcount());
    file->position = offset + read;
    const uint32_t status = read == 0 && length != 0 ? kStatusEndOfFile : kStatusSuccess;
    WriteIoStatus(io_status, status, read);
    SignalEventIfAny(call.Arg(1));
    call.Return(status, 8);
}

// NTSTATUS NtWriteFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine, PVOID ApcContext,
//     PIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length, PLARGE_INTEGER ByteOffset)
void NtWriteFile()
{
    HleCall call;
    const std::shared_ptr<KernelObject> file = FileFromHandle(call.Arg(0));
    const GuestAddress io_status = call.Arg(4);
    const uint32_t length = call.Arg(6);
    if (file != nullptr && (file->kind == KernelObject::Kind::kDevice || file->volume))
    {
        const uint32_t status =
            DeviceTransfer(*file, true, call.Arg(5), length, ResolveOffset(*file, call.Arg(7)), io_status);
        SignalEventIfAny(call.Arg(1));
        call.Return(status, 8);
        return;
    }
    if (file == nullptr || file->stream == nullptr || file->read_only)
    {
        call.Return(file == nullptr ? kStatusInvalidHandle : kStatusAccessDenied, 8);
        return;
    }
    const uint64_t offset = ResolveOffset(*file, call.Arg(7));
    std::fstream& stream = *file->stream;
    stream.clear();
    stream.seekp(static_cast<std::streamoff>(offset));
    stream.write(reinterpret_cast<const char*>(Memory().Translate(call.Arg(5))), length);
    stream.flush();
    file->position = offset + length;
    WriteIoStatus(io_status, kStatusSuccess, length);
    SignalEventIfAny(call.Arg(1));
    call.Return(kStatusSuccess, 8);
}

// NTSTATUS NtFlushBuffersFile(HANDLE FileHandle, PIO_STATUS_BLOCK IoStatusBlock)
void NtFlushBuffersFile()
{
    HleCall call;
    const std::shared_ptr<KernelObject> file = FileFromHandle(call.Arg(0));
    if (file != nullptr && file->stream != nullptr)
    {
        file->stream->flush();
    }
    WriteIoStatus(call.Arg(1), kStatusSuccess, 0);
    call.Return(kStatusSuccess, 2);
}

uint64_t SizeOf(const KernelObject& file)
{
    std::error_code ec;
    if (file.kind == KernelObject::Kind::kDirectory)
    {
        return 0;
    }
    if (file.stream != nullptr)
    {
        file.stream->flush();
    }
    const uintmax_t size = fs::file_size(file.host_path, ec);
    return ec ? 0 : static_cast<uint64_t>(size);
}

void Write64(GuestAddress address, uint64_t value)
{
    Memory().Write32(address, static_cast<uint32_t>(value));
    Memory().Write32(address + 4, static_cast<uint32_t>(value >> 32));
}

void WriteTimes(GuestAddress address, const fs::path& path)
{
    std::error_code ec;
    const fs::file_time_type time = fs::last_write_time(path, ec);
    const uint64_t nt = ec ? SystemTimeNt() : ToNtTime(time);
    for (int i = 0; i < 4; ++i)  // creation, last access, last write, change
    {
        Write64(address + i * 8, nt);
    }
}

// NTSTATUS NtQueryInformationFile(HANDLE FileHandle, PIO_STATUS_BLOCK IoStatusBlock,
//     PVOID FileInformation, ULONG Length, FILE_INFORMATION_CLASS FileInformationClass)
void NtQueryInformationFile()
{
    HleCall call;
    const std::shared_ptr<KernelObject> file = FileFromHandle(call.Arg(0));
    const GuestAddress io_status = call.Arg(1);
    const GuestAddress info = call.Arg(2);
    const uint32_t info_class = call.Arg(4);
    if (file == nullptr)
    {
        call.Return(kStatusInvalidHandle, 5);
        return;
    }
    runtime::GuestMemory& memory = Memory();
    const uint64_t size = SizeOf(*file);
    const uint64_t allocation = (size + 0xFFF) & ~0xFFFull;
    const uint32_t attributes = AttributesOf(file->host_path, file->read_only);
    uint32_t written = 0;
    switch (info_class)
    {
    case 4:  // FileBasicInformation
        WriteTimes(info, file->host_path);
        memory.Write32(info + 0x20, attributes);
        written = 0x28;
        break;
    case 5:  // FileStandardInformation
        Write64(info + 0x00, allocation);
        Write64(info + 0x08, size);
        memory.Write32(info + 0x10, 1);
        memory.Write8(info + 0x14, 0);
        memory.Write8(info + 0x15, file->kind == KernelObject::Kind::kDirectory ? 1 : 0);
        written = 0x18;
        break;
    case 14:  // FilePositionInformation
        Write64(info, file->position);
        written = 8;
        break;
    case 34:  // FileNetworkOpenInformation
        WriteTimes(info, file->host_path);
        Write64(info + 0x20, allocation);
        Write64(info + 0x28, size);
        memory.Write32(info + 0x30, attributes);
        written = 0x38;
        break;
    default:
        TraceOnce("NtQueryInformationFile", "class " + std::to_string(info_class) + " unsupported");
        WriteIoStatus(io_status, kStatusInvalidParameter, 0);
        call.Return(kStatusInvalidParameter, 5);
        return;
    }
    WriteIoStatus(io_status, kStatusSuccess, written);
    call.Return(kStatusSuccess, 5);
}

// NTSTATUS NtSetInformationFile(HANDLE FileHandle, PIO_STATUS_BLOCK IoStatusBlock,
//     PVOID FileInformation, ULONG Length, FILE_INFORMATION_CLASS FileInformationClass)
void NtSetInformationFile()
{
    HleCall call;
    const std::shared_ptr<KernelObject> file = FileFromHandle(call.Arg(0));
    const GuestAddress io_status = call.Arg(1);
    const GuestAddress info = call.Arg(2);
    const uint32_t info_class = call.Arg(4);
    if (file == nullptr)
    {
        call.Return(kStatusInvalidHandle, 5);
        return;
    }
    runtime::GuestMemory& memory = Memory();
    const uint64_t value = static_cast<uint64_t>(memory.Read32(info)) | (static_cast<uint64_t>(memory.Read32(info + 4)) << 32);
    std::error_code ec;
    switch (info_class)
    {
    case 14:  // FilePositionInformation
        file->position = value;
        break;
    case 20:  // FileEndOfFileInformation
        if (file->read_only)
        {
            call.Return(kStatusAccessDenied, 5);
            return;
        }
        if (file->stream != nullptr)
        {
            file->stream->flush();
        }
        fs::resize_file(file->host_path, value, ec);
        break;
    case 19:  // FileAllocationInformation: nothing to reserve on the host
    case 4:   // FileBasicInformation: times and attributes are not kept
        break;
    case 13:  // FileDispositionInformation
        file->delete_on_close = memory.Read8(info) != 0 && !file->read_only;
        break;
    default:
        TraceOnce("NtSetInformationFile", "class " + std::to_string(info_class) + " unsupported");
        WriteIoStatus(io_status, kStatusInvalidParameter, 0);
        call.Return(kStatusInvalidParameter, 5);
        return;
    }
    WriteIoStatus(io_status, kStatusSuccess, 0);
    call.Return(kStatusSuccess, 5);
}

// NTSTATUS NtQueryVolumeInformationFile(HANDLE FileHandle, PIO_STATUS_BLOCK IoStatusBlock,
//     PVOID FsInformation, ULONG Length, FS_INFORMATION_CLASS FsInformationClass)
void NtQueryVolumeInformationFile()
{
    HleCall call;
    const GuestAddress io_status = call.Arg(1);
    const GuestAddress info = call.Arg(2);
    const uint32_t info_class = call.Arg(4);
    runtime::GuestMemory& memory = Memory();
    // A roomy volume: 4 GiB of 16 KiB clusters, half of it free.
    const uint64_t total_units = 262144;
    const uint64_t free_units = 131072;
    switch (info_class)
    {
    case 3:  // FileFsSizeInformation
        Write64(info + 0x00, total_units);
        Write64(info + 0x08, free_units);
        memory.Write32(info + 0x10, 32);   // sectors per allocation unit
        memory.Write32(info + 0x14, 512);  // bytes per sector
        WriteIoStatus(io_status, kStatusSuccess, 0x18);
        break;
    case 7:  // FileFsFullSizeInformation
        Write64(info + 0x00, total_units);
        Write64(info + 0x08, free_units);
        Write64(info + 0x10, free_units);
        memory.Write32(info + 0x18, 32);
        memory.Write32(info + 0x1C, 512);
        WriteIoStatus(io_status, kStatusSuccess, 0x20);
        break;
    default:
        TraceOnce("NtQueryVolumeInformationFile", "class " + std::to_string(info_class) + " answered with zeros");
        memory.Fill(info, 0, call.Arg(3));
        WriteIoStatus(io_status, kStatusSuccess, call.Arg(3));
        break;
    }
    call.Return(kStatusSuccess, 5);
}

bool WildcardMatch(const std::string& pattern, const std::string& name)
{
    // '*' and '?' as in FindFirstFile, compared case-insensitively.
    size_t p = 0;
    size_t n = 0;
    size_t star = std::string::npos;
    size_t mark = 0;
    while (n < name.size())
    {
        if (p < pattern.size() && (pattern[p] == '?' ||
                                   std::toupper(static_cast<unsigned char>(pattern[p])) ==
                                       std::toupper(static_cast<unsigned char>(name[n]))))
        {
            ++p;
            ++n;
        }
        else if (p < pattern.size() && pattern[p] == '*')
        {
            star = p++;
            mark = n;
        }
        else if (star != std::string::npos)
        {
            p = star + 1;
            n = ++mark;
        }
        else
        {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*')
    {
        ++p;
    }
    return p == pattern.size();
}

// NTSTATUS NtQueryDirectoryFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine,
//     PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, PVOID FileInformation, ULONG Length,
//     FILE_INFORMATION_CLASS FileInformationClass, POBJECT_STRING FileName, BOOLEAN RestartScan)
//
// Returns one FILE_DIRECTORY_INFORMATION entry per call (names are ANSI on
// the Xbox).
void NtQueryDirectoryFile()
{
    HleCall call;
    const std::shared_ptr<KernelObject> directory = FileFromHandle(call.Arg(0));
    const GuestAddress io_status = call.Arg(4);
    const GuestAddress info = call.Arg(5);
    const uint32_t length = call.Arg(6);
    if (directory == nullptr || directory->kind != KernelObject::Kind::kDirectory)
    {
        call.Return(kStatusInvalidHandle, 10);
        return;
    }
    if (!directory->directory_listed || (call.Arg(9) & 0xFF) != 0)
    {
        std::string pattern = ReadAnsiString(call.Arg(8));
        if (pattern.empty())
        {
            pattern = "*";
        }
        directory->directory_entries.clear();
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(directory->host_path, ec))
        {
            if (WildcardMatch(pattern, entry.path().filename().string()))
            {
                directory->directory_entries.push_back(entry.path());
            }
        }
        std::sort(directory->directory_entries.begin(), directory->directory_entries.end());
        directory->directory_cursor = 0;
        directory->directory_listed = true;
    }
    if (directory->directory_cursor >= directory->directory_entries.size())
    {
        WriteIoStatus(io_status, kStatusNoMoreFiles, 0);
        call.Return(kStatusNoMoreFiles, 10);
        return;
    }
    const fs::path& entry = directory->directory_entries[directory->directory_cursor];
    const std::string name = entry.filename().string();
    const uint32_t needed = 0x40 + static_cast<uint32_t>(name.size());
    if (length < needed)
    {
        WriteIoStatus(io_status, 0x80000005, 0);  // STATUS_BUFFER_OVERFLOW
        call.Return(0x80000005, 10);
        return;
    }
    ++directory->directory_cursor;
    runtime::GuestMemory& memory = Memory();
    std::error_code ec;
    const bool is_directory = fs::is_directory(entry, ec);
    const uint64_t size = is_directory ? 0 : static_cast<uint64_t>(fs::file_size(entry, ec));
    memory.Write32(info + 0x00, 0);  // NextEntryOffset: single entry
    memory.Write32(info + 0x04, static_cast<uint32_t>(directory->directory_cursor));
    WriteTimes(info + 0x08, entry);
    Write64(info + 0x28, size);
    Write64(info + 0x30, (size + 0xFFF) & ~0xFFFull);
    memory.Write32(info + 0x38, AttributesOf(entry, directory->read_only));
    memory.Write32(info + 0x3C, static_cast<uint32_t>(name.size()));
    std::memcpy(memory.Translate(info + 0x40), name.data(), name.size());
    WriteIoStatus(io_status, kStatusSuccess, needed);
    SignalEventIfAny(call.Arg(1));
    call.Return(kStatusSuccess, 10);
}

// NTSTATUS IoCreateSymbolicLink(POBJECT_STRING SymbolicLinkName, POBJECT_STRING DeviceName)
void IoCreateSymbolicLink()
{
    HleCall call;
    const std::string link = ReadAnsiString(call.Arg(0));
    const std::string target = ReadAnsiString(call.Arg(1));
    VfsCreateSymbolicLink(link, target);
    TraceOnce("IoCreateSymbolicLink", link + " -> " + target);
    call.Return(kStatusSuccess, 2);
}

// NTSTATUS IoDeleteSymbolicLink(POBJECT_STRING SymbolicLinkName)
void IoDeleteSymbolicLink()
{
    HleCall call;
    const bool deleted = VfsDeleteSymbolicLink(ReadAnsiString(call.Arg(0)));
    call.Return(deleted ? kStatusSuccess : kStatusObjectNameNotFound, 1);
}

// NTSTATUS NtOpenSymbolicLinkObject(PHANDLE LinkHandle, POBJECT_ATTRIBUTES ObjectAttributes)
void NtOpenSymbolicLinkObject()
{
    HleCall call;
    const std::string name = ObjectName(call.Arg(1));
    const std::optional<std::string> target = VfsSymbolicLinkTarget(name);
    if (!target)
    {
        call.Return(kStatusObjectNameNotFound, 2);
        return;
    }
    auto object = std::make_shared<KernelObject>();
    object->kind = KernelObject::Kind::kSymbolicLink;
    object->xbox_path = *target;
    uint32_t handle = 0;
    {
        std::lock_guard<std::mutex> lock(State().dispatcher);
        handle = InsertHandleLocked(object);
    }
    Memory().Write32(call.Arg(0), handle);
    call.Return(kStatusSuccess, 2);
}

// NTSTATUS NtQuerySymbolicLinkObject(HANDLE LinkHandle, POBJECT_STRING LinkTarget, PULONG ReturnedLength)
void NtQuerySymbolicLinkObject()
{
    HleCall call;
    const std::shared_ptr<KernelObject> link = LookupHandle(call.Arg(0));
    if (link == nullptr || link->kind != KernelObject::Kind::kSymbolicLink)
    {
        call.Return(kStatusInvalidHandle, 3);
        return;
    }
    runtime::GuestMemory& memory = Memory();
    const GuestAddress target = call.Arg(1);
    const uint16_t capacity = memory.Read16(target + 2);
    const GuestAddress buffer = memory.Read32(target + 4);
    const uint16_t length = static_cast<uint16_t>(std::min<size_t>(link->xbox_path.size(), capacity));
    std::memcpy(memory.Translate(buffer), link->xbox_path.data(), length);
    memory.Write16(target, length);
    if (call.Arg(2) != 0)
    {
        memory.Write32(call.Arg(2), static_cast<uint32_t>(link->xbox_path.size()));
    }
    call.Return(length < link->xbox_path.size() ? 0x80000005 : kStatusSuccess, 3);
}

// The emulated hard disk: 8 GiB of 512-byte sectors.
constexpr uint64_t kDiskBytes = 8ull * 1024 * 1024 * 1024;
constexpr uint32_t kIoctlDiskGetDriveGeometry = 0x00070000;
constexpr uint32_t kIoctlDiskGetPartitionInfo = 0x00074004;

// Shared by NtDeviceIoControlFile and NtFsControlFile (same 10 arguments).
void ControlFile(const char* function)
{
    HleCall call;
    const std::shared_ptr<KernelObject> file = FileFromHandle(call.Arg(0));
    const GuestAddress io_status = call.Arg(4);
    const uint32_t code = call.Arg(5);
    const GuestAddress output = call.Arg(8);
    const uint32_t output_length = call.Arg(9);
    TraceOnce(function, (file != nullptr ? file->xbox_path : std::string("<bad handle>")) + " code 0x" + Hex(code) +
                            " in " + Hex(call.Arg(7)) + " out " + Hex(output_length));
    if (file == nullptr)
    {
        call.Return(kStatusInvalidHandle, 10);
        return;
    }
    runtime::GuestMemory& memory = Memory();
    if (output != 0 && output_length != 0)
    {
        memory.Fill(output, 0, output_length);
    }
    uint32_t information = output_length;
    if (code == kIoctlDiskGetDriveGeometry && output_length >= 24)
    {
        // DISK_GEOMETRY: Cylinders, MediaType, TracksPerCylinder, SectorsPerTrack, BytesPerSector.
        Write64(output + 0, kDiskBytes / (255ull * 63 * 512));
        memory.Write32(output + 8, 0x0C);  // FixedMedia
        memory.Write32(output + 12, 255);
        memory.Write32(output + 16, 63);
        memory.Write32(output + 20, 512);
        information = 24;
    }
    else if (code == kIoctlDiskGetPartitionInfo && output_length >= 32)
    {
        // PARTITION_INFORMATION: StartingOffset, PartitionLength, HiddenSectors, PartitionNumber, ...
        Write64(output + 0, 0);
        Write64(output + 8, kDiskBytes);
        information = 32;
    }
    WriteIoStatus(io_status, kStatusSuccess, information);
    SignalEventIfAny(call.Arg(1));
    call.Return(kStatusSuccess, 10);
}

// NTSTATUS NtDeviceIoControlFile(HANDLE FileHandle, HANDLE Event, PIO_APC_ROUTINE ApcRoutine,
//     PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, ULONG IoControlCode, PVOID InputBuffer,
//     ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength)
void NtDeviceIoControlFile()
{
    ControlFile("NtDeviceIoControlFile");
}

// NTSTATUS NtFsControlFile(...) -- the same arguments, with an FsControlCode.
void NtFsControlFile()
{
    ControlFile("NtFsControlFile");
}

// NTSTATUS IoDismountVolumeByName(POBJECT_STRING VolumeName)
void IoDismountVolumeByName()
{
    HleCall call;
    TraceOnce("IoDismountVolumeByName", ReadAnsiString(call.Arg(0)));
    call.Return(kStatusSuccess, 1);
}

}  // namespace

void RegisterFileExports(runtime::HleRegistry& registry)
{
    registry.AddExport("kernel", "NtCreateFile", &NtCreateFile);
    registry.AddExport("kernel", "NtOpenFile", &NtOpenFile);
    registry.AddExport("kernel", "NtReadFile", &NtReadFile);
    registry.AddExport("kernel", "NtWriteFile", &NtWriteFile);
    registry.AddExport("kernel", "NtFlushBuffersFile", &NtFlushBuffersFile);
    registry.AddExport("kernel", "NtQueryInformationFile", &NtQueryInformationFile);
    registry.AddExport("kernel", "NtSetInformationFile", &NtSetInformationFile);
    registry.AddExport("kernel", "NtQueryVolumeInformationFile", &NtQueryVolumeInformationFile);
    registry.AddExport("kernel", "NtQueryDirectoryFile", &NtQueryDirectoryFile);
    registry.AddExport("kernel", "IoCreateSymbolicLink", &IoCreateSymbolicLink);
    registry.AddExport("kernel", "IoDeleteSymbolicLink", &IoDeleteSymbolicLink);
    registry.AddExport("kernel", "NtOpenSymbolicLinkObject", &NtOpenSymbolicLinkObject);
    registry.AddExport("kernel", "NtQuerySymbolicLinkObject", &NtQuerySymbolicLinkObject);
    registry.AddExport("kernel", "IoDismountVolumeByName", &IoDismountVolumeByName);
    registry.AddExport("kernel", "NtDeviceIoControlFile", &NtDeviceIoControlFile);
    registry.AddExport("kernel", "NtFsControlFile", &NtFsControlFile);
}

}  // namespace rexbox::hle::kernel
