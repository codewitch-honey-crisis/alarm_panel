Import("env")
import sys

IS_WINDOWS = sys.platform.startswith("win")
IS_LINUX = sys.platform.startswith("linux")
IS_MAC = sys.platform.startswith("darwin")

print("React+TS integration enabled")
if (IS_WINDOWS):
    env.Execute("build_react.cmd")
else:
    env.Execute("build_react.sh")


print("ClASP Suite integration enabled")

env.Execute("dotnet ./build_tools/clasptree.dll ./react-web/dist ./include/httpd_content.h --prefix httpd_ --epilogue ./include/httpd_epilogue.h --state resp_arg --block httpd_send_block --expr httpd_send_expr --handlers extended --handlerfsm")