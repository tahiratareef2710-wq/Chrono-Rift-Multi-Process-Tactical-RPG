FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    gdb \
    libsfml-dev \
    libsdl2-dev \
    libglfw3-dev \
    libncurses-dev \
    && rm -rf /var/lib/apt/lists/*

COPY requirements.txt /tmp/requirements.txt

RUN grep -v '^#' /tmp/requirements.txt | grep -v '^$' | \
    xargs -r apt-get install -y

WORKDIR /app
ENV DISPLAY=:0
ENV LIBGL_ALWAYS_INDIRECT=1
COPY . .

RUN make

CMD ["bash", "run_game.sh"]