#!/bin/sh

rsync -aP \
  bob config scripts \
  root@bob:/opt/bob/run

echo "changing ownership of files"
ssh root@bob chown -R bob:bob /opt/bob/run

echo "restarting website"
ssh root@bob systemctl restart bob.web
