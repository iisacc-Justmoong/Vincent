# 문서 경로 계약

사람용 영문 진입점은 루트 `README.md`이고 에이전트 진입점은 `AGENT.md`이다. 기여 지침의 원본은 [CONTRIBUTING.md](CONTRIBUTING.md)이며 루트의 기존 경로는 이동 안내로 유지한다.

`tests/tst_windowsbuildworkflowcontract.cpp`의 문서 데이터는 `../docs/CONTRIBUTING.md`를 사용한다. 문서 이동 후에도 빌드 디렉터리, SDK 경로와 서명 정책에 관한 기존 검사가 원래 본문을 읽도록 한다. WorkspaceDocumentation의 C++23 문서 소비 검사도 실제 파일에서 같은 필수 문구를 검증한다.

문서 경로 검증은 Windows CI 실행이나 배포 완료를 의미하지 않는다. 이 보존 프로젝트의 기존 Qt/KDE 의존성과 빌드 규칙은 해당 프로젝트의 기존 계약을 따른다.
