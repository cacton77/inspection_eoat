Setting up Docker context

https://www.youtube.com/watch?v=YX2BSioWyhI

https://code.visualstudio.com/docs/containers/ssh
```bash
ssh-keygen
```

Install Docker
```bash
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh ./get-docker.sh --dry-run
```
Clone repository
```
git clone https://github.com/cacton77/inspection_eoat.git
```

Build Image
```bash
cd inspection_eoat
docker compose build
docker compose up xxxx
```
