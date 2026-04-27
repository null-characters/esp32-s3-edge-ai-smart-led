# JSON 组件别名 - 将 json 别名到 cjson
# 解决 ESP-SR 与 ESP-IDF 6.1 的兼容性问题

idf_component_register(
    INCLUDE_DIRS "include"
    REQUIRES cjson
)
