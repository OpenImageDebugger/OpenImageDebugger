FROM  frostasm/qt:qt5.12-desktop
USER root
RUN apt update -y && apt -f install -y && apt upgrade -y && apt install -y python3-dev
USER user
RUN git clone https://github.com/csantosbh/gdb-imagewatch --recurse-submodules && \
  cd gdb-imagewatch &&\
  mkdir build &&\
  cd build &&\
  qmake .. &&\
  make -j4
