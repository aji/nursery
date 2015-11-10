#!/bin/sh

cd /opt/bob/run

PYTHONPATH=$PWD:$PYTHONPATH \
STAGE=prod \
exec uwsgi \
  --socket 127.0.0.1:7456 \
  --uid bob \
  --gid bob \
  --enable-threads \
  --module bob.web \
  --callable app \
  --master
