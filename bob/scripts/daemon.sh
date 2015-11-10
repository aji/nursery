#!/bin/sh

cd /opt/bob/run

PYTHONPATH=$PWD:$PYTHONPATH \
STAGE=prod \
exec python -m bob.daemon
