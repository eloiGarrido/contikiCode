CONTIKI = ../..
CONTIKI_PROJECT = eh_wsn_main
all: $(CONTIKI_PROJECT)

#UIP_CONF_IPV6=1
CONTIKI_WITH_RIME = 1
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
APPS+=energytrace #Xin's Energy trace

include $(CONTIKI)/Makefile.include
