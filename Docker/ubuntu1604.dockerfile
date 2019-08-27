# prebuilt image of Ubuntu 16.04 with qt5
FROM  frostasm/qt:qt5.12-desktop
USER root
# adding python3-dev
RUN apt update -y && apt -f install -y && apt upgrade -y && apt install -y python3-dev
