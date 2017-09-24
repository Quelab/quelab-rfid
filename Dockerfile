FROM ubuntu:16.04
RUN mkdir -p /sketch /build /sketchbook
RUN apt-get update && apt-get install -y \
    bsdmainutils \
    curl \
    git \
    make \
    python-serial \
    screen \
    xz-utils

RUN curl -o /tmp/arduino.tar.xz https://downloads.arduino.cc/arduino-1.8.3-linux64.tar.xz
RUN tar xJf /tmp/arduino.tar.xz -C /build
RUN git clone https://github.com/sudar/Arduino-Makefile.git /build/makefiles
RUN git clone https://github.com/kak-bo-che/Arduhdlc.git /sketchbook/Arduhdlc
RUN git clone https://github.com/electronicdrops/RFIDRdm630.git /sketchbook/RFIDRdm630
RUN git clone https://github.com/pfeerick/elapsedMillis.git /sketchbook/elapsedMillis
RUN git clone https://github.com/bblanchon/ArduinoJson.git /sketchbook/ArduinoJson
WORKDIR /sketch
COPY docker-entrypoint.sh .
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8
ENTRYPOINT ["./docker-entrypoint.sh"]
