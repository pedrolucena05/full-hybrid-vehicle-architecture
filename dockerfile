FROM ubuntu:latest

# Habilita universe, PPA e instala prereqs para HTTPS/PPAs
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
       software-properties-common \
       ca-certificates \
       apt-transport-https \
       gnupg \
       wget \
 && add-apt-repository universe \
 && add-apt-repository ppa:ubuntu-toolchain-r/test -y \
 && rm -rf /var/lib/apt/lists/*

# BLOCO A: compiladores e libs de baixo nível
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
       build-essential \
       gcc-14 g++-14 \
       libcunit1 libcunit1-dev \
       libgmp-dev libmpfr-dev libmpc-dev libisl-dev \
 && update-alternatives --install /usr/bin/gcc  gcc  /usr/bin/gcc-14 100 \
 && update-alternatives --install /usr/bin/g++  g++  /usr/bin/g++-14 100 \
 && update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-14 100 \
 && rm -rf /var/lib/apt/lists/*

# BLOCO B: framework de testes e ferramentas adicionais
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
       check libsubunit-dev \
       curl git perl cpanminus \
 && rm -rf /var/lib/apt/lists/*

# 3) Instalar LCOV 2.3 manualmente
RUN curl -LO https://github.com/linux-test-project/lcov/releases/download/v2.3/lcov-2.3.tar.gz \
  && tar -xzf lcov-2.3.tar.gz \
  && cd lcov-2.3 \
  && make install \
  && cd .. \
  && rm -rf lcov-2.3 lcov-2.3.tar.gz

# Instalar módulos Perl necessários
RUN cpanm --notest Capture::Tiny DateTime Date::Parse

# Copia código e define diretório de trabalho
WORKDIR /app
COPY . /app

# Comando padrão: gera relatórios de coverage com MCDC
CMD ["make", "coverage"]