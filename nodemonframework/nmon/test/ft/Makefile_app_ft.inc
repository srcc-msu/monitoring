## Included make instructions for standard functional test
## for a separate application.

functest:
	./test.py

ifneq "$(FUNCTEST_INVERSIONS)" ""

ifeq "$(FUNCTEST_INVERTOR)" ""
FUNCTEST_INVERTOR=../common/ft_invertor
endif

inverse: functest_inverse

functest_inverse:
	@for TEST in $(FUNCTEST_INVERSIONS); do \
		$(FUNCTEST_INVERTOR) "$$TEST" || exit 1; \
	done

.PHONY: inverse functest_inverse

endif

clean:
	-rm -f -- *.pyc
	-rm -f -- *.pyo
	-rm -f ./*~

cleanall: clean

.PHONY: compile clean cleanall
.PHONY: functest

## vim:syntax=make:
