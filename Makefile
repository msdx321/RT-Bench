#general targets
all:
	@echo see README and documentation for instructions.

docs: setup-docs
	make -C ${CURDIR}/docs

clean: clean-vision clean-isolbench clean-docs clean-tacle clean-image-filters clean-utils clean-python

setup: setup-docs setup-tacle setup-image-filters setup-compile

#setup targets
setup-compile:
	@echo 'Initialization and fetching of the pinned version of the dlmalloc submodule...'
	@git submodule update --init --recursive generator/src/dlmalloc

setup-docs:
	make -C ${CURDIR}/docs setup

setup-tacle:
	@echo 'Initialization and fetching of the pinned version of the rt-tacle-bench submodule...'
	@git submodule update --init --recursive rt-tacle-bench
	@echo 'Convert the submodule README.md to a .dox file that will be included in the documentation...'
	@cd rt-tacle-bench && bash ../utils/md2dox.sh README

setup-image-filters:
	@echo 'Initialization and fetching of the pinned version of the image-filters submodule...'
	@git submodule update --init --recursive image-filters
ifndef DOCS_ONLY
	@echo 'Fetching and converting input images base...'
	@bash ${CURDIR}/image-filters/inputs/init.sh
endif
	@echo 'Convert the submodule README.md to a .dox file that will be included in the documentation...'
	@cd image-filters && bash ../utils/md2dox.sh README

#compilation targets
compile-isolbench: setup-compile
	@echo 'Compiling IsolBench'
	make -C ${CURDIR}/IsolBench/

compile-tacle: setup-tacle setup-compile
	@echo 'Compiling TACLeBench'
	make -C ${CURDIR}/rt-tacle-bench/

compile-vision: setup-compile
	@echo 'Compiling SD-VBS'
	make -C ${CURDIR}/vision/ compile

compile-image-filters: setup-image-filters setup-compile
	@echo 'Compiling image-filters'
	make -C ${CURDIR}/image-filters/

compile-utils: setup-compile
	@echo 'Compiling utils'
	make -C ${CURDIR}/utils/

#clean targets
clean-tacle:
	@echo 'Cleaning TACLeBench'
	make -C ${CURDIR}/rt-tacle-bench/ clean

clean-vision:
	@echo 'Cleaning SD-VBS'
	make -C ${CURDIR}/vision/ clean

clean-isolbench:
	@echo 'Cleaning IsolBench'
	make -C ${CURDIR}/IsolBench/ clean

clean-image-filters:
	@echo 'Cleaning image-filters'
	make -C ${CURDIR}/image-filters/ clean

clean-docs:
	@echo 'Cleaning docs'
	make -C ${CURDIR}/docs clean

clean-utils:
	@echo 'Cleaning utils'
	make -C ${CURDIR}/utils/ clean

clean-python:
	@rm -rf $(PROJ_ROOT)/.venv
# benchmark suite groups

# WCET group
setup-group-WCET: setup-tacle setup-compile

clean-group-WCET: clean-tacle

compile-group-WCET: setup-bmarks-WCET setup-compile compile-tacle

# vision group
setup-group-vision: setup-image-filters setup-compile

clean-group-vision: clean-vision clean-image-filters setup-compile

compile-group-vision: compile-vision compile-image-filters setup-compile

# interference group
setup-group-interf: setup-compile

clean-group-interf: clean-isolbench

compile-group-interf: compile-isolbench
