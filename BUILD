cc_library(
    name = "uwebsockets",
    hdrs = glob(
        ["src/**/*.h"],
        [
            "src/LocalCluster.h",
            "src/Http3*.h",
            "src/CachingApp.h",
        ],
    ),
    defines = ["UWS_NO_ZLIB"],
    includes = ["src"],
    textual_hdrs = ["src/CachingApp.h"],
    visibility = ["//visibility:public"],
    deps = ["@usockets"],
)
