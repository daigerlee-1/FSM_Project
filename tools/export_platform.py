"""Export tracked main files with only the selected platform's C branches.

Unknown build conditions are preserved. Unsupported platform expressions fail
closed. Output must be a new directory outside the repository.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess

MACRO = "PRJ_MOBILGENE_ENABLE"
ROOT = Path(__file__).resolve().parents[1]
PC_EXCLUDE = {
    "Storage/App_NvM_RamBlock.c", "Storage/App_NvM_RamBlock.h",
    "Storage/SWC_NvM.h", "Storage/drv_nvm.c",
    "Comm/lcan_input_rte.c", "Comm/lcan_input_rte.h", "Comm/lcan_version.c",
}


def git(*args):
    return subprocess.check_output(
        ["git", "-c", f"safe.directory={ROOT.as_posix()}", *args], cwd=ROOT
    ).decode("utf-8").strip()


def condition(expr, value):
    """Partially evaluate the small platform-expression grammar; no eval()."""
    expr = re.sub(r"/\*.*?\*/|//[^\n]*", "", expr, flags=re.S)
    tokens = re.findall(r"defined|[A-Za-z_]\w*|\d+[uUlL]*|==|!=|&&|\|\||[!()]", expr)
    if re.sub(r"\s+", "", expr) != "".join(tokens):
        raise ValueError(f"Unsupported platform condition: {expr}")
    position = 0

    def atom():
        nonlocal position
        token = tokens[position]
        position += 1
        if token == "(":
            result = parse(0)
            if tokens[position] != ")":
                raise ValueError(expr)
            position += 1
            return result
        if token == "!":
            operand = atom()
            return int(not operand) if isinstance(operand, int) else f"!({operand})"
        if token == "defined":
            wrapped = tokens[position] == "("
            position += int(wrapped)
            name = tokens[position]
            position += 1
            if wrapped:
                if tokens[position] != ")":
                    raise ValueError(expr)
                position += 1
            return 1 if name == MACRO else f"defined({name})"
        if token == MACRO:
            return value
        if token[0].isdigit():
            return int(token.rstrip("uUlL"))
        return token

    precedence = {"||": 1, "&&": 2, "==": 3, "!=": 3}

    def combine(left, op, right):
        if isinstance(left, int) and isinstance(right, int):
            return int({"||": bool(left or right), "&&": bool(left and right),
                        "==": left == right, "!=": left != right}[op])
        if op == "&&":
            if left == 0 or right == 0:
                return 0
            if isinstance(left, int):
                return right
            if isinstance(right, int):
                return left
        if op == "||":
            if isinstance(left, int):
                return 1 if left else right
            if isinstance(right, int):
                return 1 if right else left
        return f"({left} {op} {right})"

    def parse(minimum):
        nonlocal position
        left = atom()
        while position < len(tokens) and precedence.get(tokens[position], -1) >= minimum:
            op = tokens[position]
            position += 1
            left = combine(left, op, parse(precedence[op] + 1))
        return left

    result = parse(0)
    if position != len(tokens):
        raise ValueError(expr)
    return result


def specialize(source, value):
    source = source.replace(
        "Set to 1U for the Mobilgene/AUTOSAR target or 0U for a standalone PC build.\n"
        " * The build system may override this default definition.",
        "This snapshot contains only the " + ("Mobilgene/AUTOSAR target." if value else "PC implementation.")
    )
    # Remove the selector definition itself, not arbitrary include guards.
    source = re.sub(r"^[ \t]*#ifndef " + MACRO + r"\s*\n[ \t]*#define " +
                    MACRO + r"[^\n]*\n[ \t]*#endif[^\n]*\n", "", source, flags=re.M)
    logical = re.findall(r"(?:[^\n]*\\\n)*[^\n]*(?:\n|$)", source)
    logical = [line for line in logical if line]
    position = 0

    def directive(line):
        return re.match(r"^[ \t]*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)",
                        line.replace("\\\n", " "), re.S)

    def block():
        nonlocal position
        output = []
        while position < len(logical):
            match = directive(logical[position])
            if match and match[1] in ("elif", "else", "endif"):
                break
            if not match:
                output.append(logical[position])
                position += 1
                continue
            branches = []
            while True:
                opening = logical[position]
                opening_match = directive(opening)
                position += 1
                body = block()
                branches.append((opening, opening_match, body))
                if position >= len(logical):
                    raise ValueError("Unterminated conditional")
                closing = directive(logical[position])
                if closing[1] == "endif":
                    ending = logical[position]
                    position += 1
                    break
                if opening_match[1] == "else":
                    raise ValueError("Branch after else")
            if not any(MACRO in item[0] for item in branches):
                output.extend(opening + body for opening, _, body in branches)
                output.append(ending)
                continue
            emitted = False
            for opening, match, body in branches:
                if match[1] == "else":
                    result = 1
                elif MACRO not in opening:
                    result = match[2].strip()
                    if match[1] == "ifdef":
                        result = f"defined({result})"
                    elif match[1] == "ifndef":
                        result = f"!defined({result})"
                elif match[1] in ("ifdef", "ifndef"):
                    result = int(match[1] == "ifdef")
                else:
                    result = condition(match[2].strip(), value)
                if isinstance(result, int) and not result:
                    continue
                if isinstance(result, int):
                    output.append(("#else\n" if emitted else "") + body)
                    break
                output.append(("#elif " if emitted else "#if ") + result + "\n" + body)
                emitted = True
            if emitted:
                output.append("#endif\n")
        return "".join(output)

    result = block()
    if position != len(logical):
        raise ValueError("Unmatched conditional")
    if MACRO in result:
        raise ValueError("Unresolved platform selector")
    return result


def export(platform, destination):
    destination = destination.resolve()
    if destination == ROOT or ROOT in destination.parents or destination.exists():
        raise ValueError("Output must be a new directory outside this repository")
    value = int(platform == "mobilgene")
    source_commit = git("rev-parse", "HEAD")
    files = git("ls-files", "-z").split("\0")
    # Generate everything in memory before writing any output.
    generated = {}
    excluded = []
    for name in files:
        if not name or name.startswith("tools/"):
            continue
        if (value and (name.startswith(("FSM_Core/tests/", "Storage/tests/")) or name == "Storage/drv_nvm_pc.c")) or (
            not value and (name in PC_EXCLUDE or (Path(name).name.startswith("SWC_") and name.endswith(".c")))
        ):
            excluded.append(name)
            continue
        data = (ROOT / name).read_bytes()
        if name.endswith((".c", ".h")):
            text = data.decode("utf-8-sig").replace("\r\n", "\n")
            generated[name] = specialize(text, value).encode("utf-8")
        else:
            generated[name] = data
    mode = "MOBILGENE/AUTOSAR" if value else "PC 테스트"
    generated["README.md"] = (
        f"# FSM Project — {mode} 전용\n\n"
        f"공통 원본: main `{source_commit}`. 이 브랜치는 main에서 자동 추출했습니다.\n\n"
        "플랫폼 선택 매크로와 반대 플랫폼 분기는 제거되어 있습니다. 플랫폼 선택 옵션을 지정하지 않습니다.\n"
        "기능 변경은 main에서 수행한 뒤 다시 추출합니다. 전용 브랜치 전체를 main에 병합하지 않습니다.\n"
        "운영 방식 및 갱신 명령은 main의 README와 tools/export_platform.py를 참고합니다.\n\n"
        + ("타깃 BSW/RTE/OS, MCAL 및 링커 설정을 외부 프로젝트에서 제공해야 합니다.\n"
           "PC 회귀 테스트와 추출 도구는 이 브랜치에서 제외합니다.\n" if value else
           "FSM_Core/tests/run_tests.ps1로 FSM 회귀 테스트를 실행합니다. Storage/drv_nvm_pc.c는 RAM 저장소입니다.\n"
           "AUTOSAR 전용 SWC, RTE 입출력 어댑터 및 기존 NvM RAM 연계 파일은 제외합니다.\n"
           "전체 애플리케이션 실행에는 task.h/task.c, drv_console 및 CAN 서비스 대역이 추가로 필요합니다.\n"
           "LED I2C 대역은 실제 완료 알림이나 IC 프로토콜을 모사하지 않습니다.\n")
        + "\n기존 모듈 README/WARNING은 검토 이력이며 main의 통합 구성을 기준으로 한 설명이 포함됩니다.\n"
          "분기 추출은 기존 WARNING의 기능 결함을 자동으로 해결하지 않습니다.\n"
    ).encode("utf-8")
    generated["PLATFORM_SOURCE.json"] = (json.dumps(
        {"source_branch": "main", "source_commit": source_commit,
         "platform": platform, "excluded_files": sorted(excluded)}, indent=2) + "\n").encode()
    for name, data in generated.items():
        output = destination / name
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(data)
    print(f"Exported {len(generated)} files: {platform}, source {source_commit}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", choices=("mobilgene", "pc"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if git("branch", "--show-current") != "main" or git("status", "--porcelain"):
        parser.error("Export requires a clean, committed main checkout")
    export(args.platform, args.output)
