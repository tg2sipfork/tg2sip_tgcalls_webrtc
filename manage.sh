#!/bin/bash

set -e

IMAGE="tg-dev:latest"
CONTAINER="tg-dev"

function print_help() {
    echo "Usage: ./manage.sh [command]"
    echo "Commands:"
    echo "  build    - Build the docker image"
    echo "  up       - Start the container (docker compose up -d)"
    echo "  shell    - Enter the container shell"
    echo "  restart  - Recreate container and enter shell"
    echo "  stop    - stop container "
}

case "$1" in
    build)
        echo "Building image $IMAGE..."
        docker build --memory=25g -t "$IMAGE" .
        ;;
    up)
        echo "Starting container..."
        docker compose up -d
        ;;
    shell)
        docker exec -it "$CONTAINER" bash
        ;;
    stop)
        docker rm -f "$CONTAINER"
        ;;
    restart)
        echo "Restarting container..."
        docker rm -f "$CONTAINER" 2>/dev/null || true
        docker compose up -d
        docker exec -it "$CONTAINER" bash
        ;;
    *)
        print_help
        exit 1
        ;;
esac













