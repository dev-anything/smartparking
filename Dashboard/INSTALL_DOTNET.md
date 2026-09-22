# .NET 8.0 SDK 설치 가이드 (macOS)

## 방법 1: Homebrew (권장)

Homebrew가 이미 설치되어 있다면:

```bash
# Apple Silicon (M1/M2/M3)
brew install --cask dotnet-sdk

# Intel
brew install --cask dotnet-sdk

# 또는 .NET 8.x 직접 설치
brew install dotnet@8
```

## 방법 2: 직접 다운로드

1. https://dot.net/download 접속
2. **.NET 8.0 SDK** 의 macOS (Apple Silicon / Intel) 설치 파일 다운로드
3. `.pkg` 파일 더블클릭하여 설치

## 설치 확인

```bash
dotnet --version
# 8.0.x 가 출력되면 성공

dotnet --list-sdks
# 8.0.x 가 보이면 성공
```

## 자주 발생 하는 문제

### `dotnet : command not found`
- `.NET`을 `.zshrc` PATH 에 추가해야 할 수도 있습니다.
- 보통 `brew install --cask dotnet-sdk` 는 자동으로 PATH 등록하지만,
  수동 설치를 했다면 `~/.dotnet` PATH 를 추가:

```bash
echo 'export PATH=$PATH:$HOME/.dotnet' >> ~/.zshrc
source ~/.zshrc
```

### `BUILD FAILED — A compatible .NET SDK was not found`
- 다중 버전이 설치되어 있고 프로젝트가 다른 버전을 요구할 수 있음.
- `global.json` 을 만들거나, 원하는 SDK 설치:

```bash
brew install dotnet@8
```
