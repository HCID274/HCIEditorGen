import json
import pathlib
import sys

import unreal

"""
HCI Ability Kit Python 钩子脚本喵。
该脚本在 C++ 解析 .hciabilitykit 文件后被调用，用于执行额外的逻辑校验或数据处理喵。
"""

def main() -> None:
    # 检查命令行参数，第一个参数应为被解析的源文件路径喵
    if len(sys.argv) < 2:
        raise RuntimeError("Missing source file argument for AbilityKit Python hook")

    source_file = pathlib.Path(sys.argv[1])
    # 确保源文件确实存在喵
    if not source_file.exists():
        raise RuntimeError(f"AbilityKit source file not found: {source_file}")

    # 读取并解析 JSON 数据喵
    data = json.loads(source_file.read_text(encoding="utf-8"))
    
    # 这是一个特殊的测试字段：如果在 JSON 中设置了 __force_python_fail__ 为 true，
    # 脚本会故意抛出异常以模拟验证失败的场景喵。
    if data.get("__force_python_fail__", False):
        raise RuntimeError("Forced Python failure: __force_python_fail__ == true")

    # 在虚幻编辑器的输出日志中记录处理成功的消息喵
    unreal.log(f"[HCIAbilityKit] Python hook passed: {source_file}")


if __name__ == "__main__":
    # 执行主函数喵
    main()
