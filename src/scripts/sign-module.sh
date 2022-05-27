#!/bin/sh
MOK_DIR=$HOME/vms/vmware-keys
sudo /usr/src/linux-headers-`uname -r`/scripts/sign-file sha256 $MOK_DIR/MOK.priv $MOK_DIR/MOK.der $1