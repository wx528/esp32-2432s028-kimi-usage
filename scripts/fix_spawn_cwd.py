# 修复 PlatformIO/SCons 在 Windows 上调用 cmd.exe 时当前工作目录丢失的问题。
# 在每条命令前显式加上 "cd /d <项目目录> &&"，确保相对路径能正确解析。
# 仅 Windows 生效；Linux/macOS（如 CI）的 shell 没有这个问题，且 `cd /d` 是 cmd.exe 语法会报错。
import subprocess
import sys

Import("env")

if sys.platform == "win32":
    _project_dir = env.subst("$PROJECT_DIR")

    def _spawn_with_cwd(sh, escape, cmd, args, env):
        # args[0] 通常是程序名（已被 SCons 转义过），直接用空格拼接成命令行
        command_line = " ".join(args)
        wrapped = f'cd /d "{_project_dir}" && {command_line}'
        return subprocess.call(wrapped, shell=True, env=env)

    env['SPAWN'] = _spawn_with_cwd
