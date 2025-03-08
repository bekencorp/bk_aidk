set(incs .)
set(srcs "")

list(APPEND srcs
	bk_factory_config.c
)


armino_component_register(
    SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES bk_common
)

