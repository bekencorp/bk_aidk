set(incs .)
set(srcs "")

list(APPEND srcs
	led_blink.c
  led_app.c
)


armino_component_register(
    SRCS "${srcs}"
	INCLUDE_DIRS "${incs}"
	PRIV_REQUIRES bk_common
)

