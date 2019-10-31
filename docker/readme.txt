To build docker containers, just run:

./build-docker

To push the containers to the ACR add "push" argument to the script run:

./build-docker push

To run server do this (assuming the name of the on-prem speech Docker container is ignite, and assuming this name is in config.json):

docker run --rm -d --name mrcpserver --link ignite:ignite -v /home/mradmila/unimrcp/config.json:/usr/local/unimrcp/conf/config.json -p 8060:8060/udp -p 8060:8060/tcp -p 1544:1544 -p 1554:1554 skyman.azurecr.io/scratch/onprem/unimrcp:server

Logs can be extracted using: docker logs mrcpserver

To run the ASR client do this:

docker run --rm -ti --name asrclient --net=host skyman.azurecr.io/scratch/onprem/unimrcp:asrclient

and then inside run at the prompt (">"): run grammar.xml johnsmith-16kHz.pcm

Enjoy!