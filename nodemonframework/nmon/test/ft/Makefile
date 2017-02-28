FUNCTESTS = \
	ft1a_start_and_send \
	ft1b_cac_mode \
	ft2a_os_sensors

functest:
	@for TEST in $(FUNCTESTS); do \
		$(MAKE) -C "$$TEST" functest || exit 1; \
	done

all: functest

## This shall be subset of FUNCTESTS.
FUNCTESTS_INVERSE = \
	ft1a_start_and_send \
	ft1b_cac_mode

functest_inverse:
	@for TEST in $(FUNCTESTS_INVERSE); do \
		$(MAKE) -C "$$TEST" functest_inverse || exit 1; \
	done

inverse: functest_inverse

cleantest:
	@for TEST in $(FUNCTESTS) functests; do \
		$(MAKE) -C "$$TEST" clean cleanall; \
	done

clean: cleantest

.PHONY: all functest inverse functest_inverse clean cleantest
