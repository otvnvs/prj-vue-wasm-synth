#!/bin/bash
PORT=2345
WEBROOT=./dist
darkhttpd $WEBROOT \
  --mimetypes ./mime.types \
  --header "Access-Control-Allow-Origin: *" \
  --header "Cross-Origin-Opener-Policy: same-origin" \
  --header "Cross-Origin-Embedder-Policy: require-corp" \
  --header "Cross-Origin-Resource-Policy: cross-origin" \
  --port $PORT
