set(srcs "main.c"
        "board.c"
        "hsl_github.c"
        "hsl.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS  ".")
