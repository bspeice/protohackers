FROM gcc:12
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN echo "deb http://deb.debian.org/debian bullseye-backports main" > /etc/apt/sources.list.d/bullseye-backports.list \
 && apt-get update \
 && apt-get install --assume-yes --no-install-recommends --quiet -t bullseye-backports \
    clang-format \
    clang-tidy \
    cmake \
    ninja-build \
    python3-pip \
    python3-virtualenv \
    sudo \
 && apt-get clean all


RUN groupadd --gid $USER_GID $USERNAME \
 && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
 && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
 && chmod 0440 /etc/sudoers.d/$USERNAME