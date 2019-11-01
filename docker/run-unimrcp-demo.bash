#!/bin/bash -ex

SPEECH_CONTAINER_NAME="${SPEECH_CONTAINER_NAME:-localsr}"
SPEECH_CONTAINER_PORT="${SPEECH_CONTAINER_PORT:-5000}"
SPEECH_CR="${SPEECH_CR:-containerpreview.azurecr.io}"
SPEECH_REPO="${SPEECH_REPO:-microsoft/cognitive-services-speech-to-text}"
SPEECH_TAG="${SPEECH_TAG:-1.2.0-amd64-en-us-preview}"
SPEECH_REGION="${SPEECH_REGION:-invalid}"
SPEECH_APIKEY="${SPEECH_APIKEY:-invalid}"
SPEECH_MAX_DECODERS="${SPEECH_MAX_DECODERS:-20}"
UNIMRCP_CR="${UNIMRCP_CR:-skyman.azurecr.io}"
UNIMRCP_REPO="${UNIMRCP_REPO:-scratch/onprem/unimrcp}"
UNIMRCP_CLIENT_TAG="${UNIMRCP_CLIENT_TAG:-asrclient}"
UNIMRCP_SERVER_TAG="${UNIMRCP_SERVER_TAG:-server}"

# parsing command line arguments:
while [[ $# > 0 ]]; do
    key="$1"
    case $key in
        -h|--help)
            echo "Usage: run-unimrcp-demo [run_options]"
            echo "Options:"
            echo "  -r|--region <region> - speech region to use"
            echo "  -k|--apikey <key> - api key to use"
            exit 1
            ;;
        -r|--region)
            SPEECH_REGION="$2"
            shift
            ;;
        -k|--apikey)
            SPEECH_APIKEY="$2"
            shift
            ;;
        *)
            echo "Unknown option $key -- Ignoring"
            ;;
    esac
    shift # past argument or value ($1)
done

# Computed values
SOURCE_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
SPEECH_BILLING="https://${SPEECH_REGION}.api.cognitive.microsoft.com"
SPEECH_ENDPOINT="${SPEECH_CONTAINER_NAME}:${SPEECH_CONTAINER_PORT}"
SPEECH_DCR="${SPEECH_CR}/${SPEECH_REPO}:${SPEECH_TAG}"
UNIMRCP_SERVER_DCR="${UNIMRCP_CR}/${UNIMRCP_REPO}:${UNIMRCP_SERVER_TAG}"
UNIMRCP_CLIENT_DCR="${UNIMRCP_CR}/${UNIMRCP_REPO}:${UNIMRCP_CLIENT_TAG}"
sed "s/YourSpeechContainerEndPoint/$SPEECH_ENDPOINT/g" ${SOURCE_DIR}/config.json > ${SOURCE_DIR}/config.json.rej

# Cleanup previous instances of the containers
docker kill ${SPEECH_CONTAINER_NAME} unimrcpserver asrclient || echo "No containers running, that's OK"

# Make sure mandatory arguments are passed properly
if [[ "${SPEECH_APIKEY}" == "invalid" ]]; then
    echo "Must supply speech api key!"
    exit 1
fi

if [[ "${SPEECH_REGION}" == "invalid" ]]; then
    echo "Must supply speech region!"
    exit 1
fi

# Run docker images
docker pull ${SPEECH_DCR}
docker pull ${UNIMRCP_SERVER_DCR}
docker pull ${UNIMRCP_CLIENT_DCR}
docker run --rm -d --name ${SPEECH_CONTAINER_NAME} -e DECODER_MAX_COUNT=${SPEECH_MAX_DECODERS} -p ${SPEECH_CONTAINER_PORT}:5000 "${SPEECH_DCR}" eula=accept billing=${SPEECH_BILLING} apikey=${SPEECH_APIKEY}
docker run --rm -d --name unimrcpserver --link ${SPEECH_CONTAINER_NAME}:${SPEECH_CONTAINER_NAME} -v ${SOURCE_DIR}/config.json.rej:/usr/local/unimrcp/conf/config.json -p 8060:8060/udp -p 8060:8060/tcp -p 1544:1544 -p 1554:1554 "${UNIMRCP_SERVER_DCR}"

echo "RUN THE FOLLOWING COMMAND> run grammar.xml whatstheweatherlike.wav"
docker run --rm -ti --name asrclient --net=host "${UNIMRCP_CLIENT_DCR}"

