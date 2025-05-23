set(incs .)
set(srcs "")

list(APPEND srcs
	countdown.c
  countdown_app.c
)


armino_component_register(
    SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES bk_common bk_factory_config
)

