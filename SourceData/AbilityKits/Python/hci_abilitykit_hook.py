import json
import pathlib
import sys
import traceback

try:
    import unreal
except Exception:
    unreal = None


def _write_response(output_file: pathlib.Path, payload: dict) -> None:
    output_file.parent.mkdir(parents=True, exist_ok=True)
    output_file.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")


def _error_payload(code: str, field: str, reason: str, hint: str, detail: str = "") -> dict:
    return {
        "ok": False,
        "patch": {},
        "error": {
            "code": code,
            "field": field,
            "reason": reason,
            "hint": hint,
            "detail": detail,
        },
        "audit": [
            {
                "level": "error",
                "code": code,
                "message": reason,
            }
        ],
    }


def main() -> None:
    if len(sys.argv) < 3:
        raise RuntimeError("Expected arguments: <source_file> <response_file>")

    source_file = pathlib.Path(sys.argv[1])
    response_file = pathlib.Path(sys.argv[2])

    if not source_file.exists():
        _write_response(
            response_file,
            _error_payload(
                "E1005",
                "file",
                "AbilityKit source file not found",
                "Check source path and file existence",
                str(source_file),
            ),
        )
        return

    try:
        data = json.loads(source_file.read_text(encoding="utf-8"))

        if data.get("__force_python_fail__", False):
            _write_response(
                response_file,
                _error_payload(
                    "E3001",
                    "python_hook",
                    "Forced Python failure: __force_python_fail__ == true",
                    "Set __force_python_fail__ to false",
                ),
            )
            return

        response = {"ok": True, "patch": {}, "audit": []}

        if "display_name_ai" in data:
            if not isinstance(data["display_name_ai"], str):
                _write_response(
                    response_file,
                    _error_payload(
                        "E1003",
                        "display_name_ai",
                        "Invalid field type",
                        "display_name_ai must be a string",
                    ),
                )
                return

            response["patch"]["display_name"] = data["display_name_ai"]
            response["audit"].append(
                {
                    "level": "info",
                    "code": "A1001",
                    "message": "Applied display_name from display_name_ai",
                }
            )
        else:
            response["audit"].append(
                {
                    "level": "info",
                    "code": "A1002",
                    "message": "No display_name_ai provided; keep parser display_name",
                }
            )

        if "representing_mesh_ai" in data:
            if not isinstance(data["representing_mesh_ai"], str):
                _write_response(
                    response_file,
                    _error_payload(
                        "E1003",
                        "representing_mesh_ai",
                        "Invalid field type",
                        "representing_mesh_ai must be a string",
                    ),
                )
                return

            response["patch"]["representing_mesh"] = data["representing_mesh_ai"]
            response["audit"].append(
                {
                    "level": "info",
                    "code": "A1003",
                    "message": "Applied representing_mesh from representing_mesh_ai",
                }
            )

        _write_response(response_file, response)
        if unreal:
            unreal.log(f"[HCIAbilityKit] Python hook passed: {source_file}")
    except Exception as ex:
        _write_response(
            response_file,
            _error_payload(
                "E3001",
                "python_hook",
                "Unhandled Python exception",
                "Inspect detail traceback and fix script",
                f"{ex}\n{traceback.format_exc()}",
            ),
        )


if __name__ == "__main__":
    main()
