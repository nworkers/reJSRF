# 작업 지시: 픽셀 셰이더 정의 상수 (issue #78)

설계: [20261017-i078](../design/20261017-i078-psh-definition-constants.md)

1. `d3d8_shader`: SetPixelShaderConstant가 쓴 레지스터의 비트 마스크, `PixelShaderConstantsWritten()`, `ForgetPixelShaderConstants()`.
2. `d3d8_state`: `D3DDevice_SetPixelShader`가 마스크를 지운다.
3. `d3d8_psh`: `StageConstants`가 마스크를 받아, 쓰인 레지스터만 런타임 값을 쓴다.
4. `d3d8_draw`: 마스크를 넘긴다.
5. `rexbox_d3d8_probe`: 쓰이지 않은 매핑 레지스터는 정의 값을 쓰는지 검사.
6. 검증: 빌드, 회귀 도구, 데모 프레임 비교. 문서: 작업 로그.

---

# Work order: pixel shader definition constants (issue #78)

Design: [20261017-i078](../design/20261017-i078-psh-definition-constants.md)

1. `d3d8_shader`: a bit mask of the registers SetPixelShaderConstant wrote, `PixelShaderConstantsWritten()`, `ForgetPixelShaderConstants()`.
2. `d3d8_state`: `D3DDevice_SetPixelShader` clears the mask.
3. `d3d8_psh`: `StageConstants` takes the mask and uses runtime values only for written registers.
4. `d3d8_draw`: passes the mask.
5. `rexbox_d3d8_probe`: checks that an unwritten mapped register takes the definition's value.
6. Verification: builds, regression tools, demo frame comparison. Documents: work log.
