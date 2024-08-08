Setting up Docker context

https://www.youtube.com/watch?v=YX2BSioWyhI

https://code.visualstudio.com/docs/containers/ssh
```bash
ssh-keygen
```

## Install Ubuntu Server 22.04

Install Raspberry Pi Imager

[Windows or macOS](https://www.raspberrypi.com/software/)

Debian
```bash
sudo apt install rpi-imager
```

1) Select operating system:
Choose OS -> Other General Purpose OS -> Ubuntu -> Ubuntu Server 22.04 LTS (64-bit).

2) Select storage device for OS.

3) Click on settings icon in bottom right and use [this configuration](docs/imager_settings.png).

## EOAT Setup


### Install docker
[Install Docker](https://docs.docker.com/engine/install/ubuntu/#install-using-the-convenience-script)
```bash
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh ./get-docker.sh
sudo groupadd docker
sudo usermod -aG docker $USER
newgrp docker
docker run hello-world
```
If install was successful, you should see [this output](docs/docker_install_output.png) after running the last command.


### Clone repository
```
git clone https://github.com/cacton77/inspection_eoat.git
```

Build Image
```bash
cd inspection_eoat
docker compose build
docker compose up xxxx
```
