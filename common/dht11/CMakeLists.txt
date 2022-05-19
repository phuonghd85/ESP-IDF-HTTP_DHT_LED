set(app_src dht11.c)
set(pri_req) #dùng thư viện nào trong idf/component thì thêm vào đây
idf_component_register(SRCS ${app_src}
                   INCLUDE_DIRS "."
                   REQUIRES ${pri_req})