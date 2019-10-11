To build docker containers, just run:

./build-docker

To push the containers to the ACR add "push" argument to the script run:

./build-docker push

To run server do this:

docker run --rm -d --name mrcpserver -v /home/mradmila/unimrcp_server_files/config.json:/usr/local/unimrcp/conf/config.json -v /home/mradmila/unimrcp_server_files/unimrcpserver.xml:/usr/local/unimrcp/conf/unimrcpserver.xml -p 8060:8060/udp -p 8060:8060/tcp -p 1544:1544 -p 1554:1554 skyman.azurecr.io/scratch/onprem/unimrcp:server

Logs can be extracted using: docker logs mrcpserver

To run the ASR client do this:

docker run --rm -ti --name asrclient --entrypoint ./umc --net=host skyman.azurecr.io/scratch/onprem/unimrcp:asrclient

and then inside run at the prompt (">"): run recog

Enjoy!