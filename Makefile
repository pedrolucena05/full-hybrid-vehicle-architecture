# — Diretories —
SRC_DIR      := src
TEST_DIR     := test
BINSRC       := bin
BINTEST := $(BINSRC)/test
COVERAGE_DIR := coverage

# — Compiler and flags —
CC           := gcc

# Coverage flags: standard gcov instrumentation
# -fprofile-arcs -ftest-coverage (--coverage) são necessários para gcov/LCOV :contentReference[oaicite:0]{index=0}
CFLAGS        := -g -O0 --coverage -fprofile-arcs -ftest-coverage \
                  -fkeep-inline-functions -fkeep-static-functions

# Add MC/DC instrumentation flag (GCC‑14 patch: -fcondition-coverage) 
CFLAGS_MCDC   := $(CFLAGS) -fcondition-coverage

# Flags para testes (link com Check framework, subunit etc.)
CFLAGS_TEST   := $(CFLAGS_MCDC) -DTESTING
LDLIBS        := --coverage -lcheck -pthread -lrt -lm -lsubunit -ldl -lgcov

MODULES      := vmu ev iec
EXECS        := $(addprefix $(BINSRC)/, $(MODULES))

TMUX_SESSION := sistema

# — Source files and objects for each module —
VMU_SRCS := $(filter-out src/vmu/main.c,$(wildcard src/vmu/*.c))
VMU_OBJS := $(patsubst src/vmu/%.c,$(BINSRC)/%.o,$(VMU_SRCS))
VMU_MAIN := $(BINSRC)/main_vmu.o
VMU_APP  := $(VMU_OBJS) $(VMU_MAIN)

EV_SRCS := $(filter-out src/ev/main.c,$(wildcard src/ev/*.c))
EV_OBJS := $(patsubst src/ev/%.c,$(BINSRC)/%.o,$(EV_SRCS))
EV_MAIN := $(BINSRC)/main_ev.o
EV_APP  := $(EV_OBJS) $(EV_MAIN)

IEC_SRCS := $(filter-out src/iec/main.c,$(wildcard src/iec/*.c))
IEC_OBJS := $(patsubst src/iec/%.c,$(BINSRC)/%.o,$(IEC_SRCS))
IEC_MAIN := $(BINSRC)/main_iec.o
IEC_APP  := $(IEC_OBJS) $(IEC_MAIN)

TEST_SRCS := $(wildcard $(TEST_DIR)/vmu/test_vmu.c \
                       $(TEST_DIR)/ev/test_ev.c \
                       $(TEST_DIR)/iec/test_iec.c)

# Test objects: bin/test_vmu.o, bin/test_ev.o, bin/test_iec.o
TEST_OBJS := $(patsubst $(TEST_DIR)/%/test_%.c,$(BINSRC)/test_%.o,$(TEST_SRCS))

TESTS := $(BINTEST)/test_vmu $(BINTEST)/test_ev $(BINTEST)/test_iec

# — All Makefile commands —
.PHONY: all test coverage run show clean kill mcdc
.SECONDARY: $(BINSRC)/%.o

# — Standard target: compile project —
all: $(EXECS)

# — Create bin diretory if necessary —
$(BINSRC):
	mkdir -p $@

$(BINTEST): | $(BINSRC)
	mkdir -p $@

# — Compilation rules for each module —
$(BINSRC)/%.o: src/vmu/%.c | $(BINSRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINSRC)/%.o: src/ev/%.c | $(BINSRC)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINSRC)/%.o: src/iec/%.c | $(BINSRC)
	$(CC) $(CFLAGS) -c $< -o $@

# — Compile main of each module —
$(BINSRC)/main_%.o: src/%/main.c | $(BINSRC)
	# compila sem cobertura para não gerar .gcno/.gcda
	$(CC) -g -O0 -c $< -o $@

# — Link executables —
$(BINSRC)/vmu: $(VMU_APP)
	$(CC) $^ -o $@ $(CFLAGS) $(LDLIBS)

$(BINSRC)/ev: $(EV_APP)
	$(CC) $^ -o $@ $(CFLAGS) $(LDLIBS)

$(BINSRC)/iec: $(IEC_APP)
	$(CC) $^ -o $@ $(CFLAGS) $(LDLIBS)

# 1) Rules for each test object
$(BINSRC)/test_vmu.o: $(TEST_DIR)/vmu/test_vmu.c | $(BINSRC)
	$(CC) $(CFLAGS_TEST) -c $< -o $@

$(BINSRC)/test_ev.o: $(TEST_DIR)/ev/test_ev.c | $(BINSRC)
	$(CC) $(CFLAGS_TEST) -c $< -o $@

$(BINSRC)/test_iec.o: $(TEST_DIR)/iec/test_iec.c | $(BINSRC)
	$(CC) $(CFLAGS_TEST) -c $< -o $@

# 2) Link: generate bin/test/test_vmu, bin/test/test_ev, bin/test/test_iec
#    Use each test object + already compiled objects
$(BINTEST)/test_vmu: $(BINSRC)/test_vmu.o $(VMU_OBJS) | $(BINTEST)
	$(CC) $^ -o $@ $(CFLAGS_TEST) $(LDLIBS)

$(BINTEST)/test_ev:  $(BINSRC)/test_ev.o  $(EV_OBJS) | $(BINTEST)
	$(CC) $^ -o $@ $(CFLAGS_TEST) $(LDLIBS)

$(BINTEST)/test_iec: $(BINSRC)/test_iec.o $(IEC_OBJS) | $(BINTEST)
	$(CC) $^ -o $@ $(CFLAGS_TEST) $(LDLIBS)

# — Run tests —
test: $(TESTS)
	@echo ">>> Entrou no target test <<<"
	@for t in $^; do \
		echo "Executando $$t…"; \
		./$$t; \
	done

LCOV_GCOV_TOOL := gcov-14

# — Coverage report —
coverage:
	@echo ">>> Limpando e compilando com MC/DC…"
	@$(MAKE) clean
	@$(MAKE) CFLAGS="$(CFLAGS_MCDC)" all test

	@echo ">>> Zerando contadores…"
	lcov --zerocounters --directory bin/ \
	     --rc branch_coverage=1 \
	     --rc mcdc_coverage=1

	@echo ">>> Capturando baseline (.gcno)…"
	lcov --capture --initial \
	     --gcov-tool gcov-14 \
	     --directory bin/ \
	     --output-file coverage_base.info \
	     --rc branch_coverage=1 \
	     --rc mcdc_coverage=1

	@echo ">>> Executando testes para gerar .gcda…"
	for t in bin/test/*; do ./$$t; done

	@echo ">>> Capturando cobertura de teste (.gcda)…"
	lcov --capture \
	     --gcov-tool gcov-14 \
	     --directory bin/ \
	     --output-file coverage_test.info \
	     --rc branch_coverage=1 \
	     --rc mcdc_coverage=1

	@echo ">>> Mesclando baseline + teste…"
	lcov -a coverage_base.info \
	     -a coverage_test.info \
	     -o coverage.info \
	     --rc branch_coverage=1 \
	     --rc mcdc_coverage=1

	@echo ">>> Filtrando arquivos irrelevantes…"
	lcov --remove coverage.info '/usr/*' '*/test_*' \
	     --output-file coverage_filtered.info \
	     --rc branch_coverage=1 \
	     --rc mcdc_coverage=1

	@echo ">>> Gerando relatório HTML com MC/DC…"
	genhtml coverage_filtered.info \
	        --branch-coverage \
	        --mcdc-coverage \
	        --output-directory coverage_html

	@echo "Relatório disponível em coverage_html/index.html"	        


# — Run aplication at tmux —
run: all
	@tmux new-session -d -s $(TMUX_SESSION) -n vmu './$(BINSRC)/vmu' \
		&& tmux split-window -v './$(BINSRC)/ev' \
		&& tmux split-window -h './$(BINSRC)/iec' \
		&& tmux select-layout tiled \
		&& tmux attach

# — Open coverage report —
show:
	xdg-open $(COVERAGE_DIR)/index.html || echo "Falha ao abrir o relatório de cobertura"

# — Clean executable objects —
clean:
	
	rm -rf $(BINSRC) $(BINTEST) bin coverage_html
	rm -f coverage.info \
	      coverage_base.info \
	      coverage_filtered.info \
	      coverage_test.info \
	      out.txt

# — End tmux session —
kill:
	@tmux kill-session -t $(TMUX_SESSION) || echo "Nenhuma sessão tmux para encerrar"


DOCKER_IMAGE   := vmu_controller
DOCKER_CONTAINER := vmu_controller_container

.PHONY: docker-build

docker-build:
	docker build -t $(DOCKER_IMAGE) .

.PHONY: docker-run

docker-run: docker-build
	# Remove container antigo, se existir
	-docker rm -f $(DOCKER_CONTAINER)
	# Cria e inicia novo container em modo interativo
	docker run --name $(DOCKER_CONTAINER) \
	  -v $(CURDIR):/workspace \
	  -w /workspace \
	  --entrypoint /bin/bash \
	  -it $(DOCKER_IMAGE)

.PHONY: docker-clean

docker-clean:
	# Remove container parado
	-docker rm -f $(DOCKER_CONTAINER)
	# Remove imagem
	-docker rmi $(DOCKER_IMAGE)