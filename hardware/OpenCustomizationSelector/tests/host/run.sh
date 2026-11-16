#!/bin/sh
# Host test for ModemState (pure Java, no Android). Usage: tests/host/run.sh
set -e
D=$(cd "$(dirname "$0")/../.." && pwd)
O=$(mktemp -d)
javac -d "$O" "$D/src/com/sonymobile/customizationselector/ModemState.java" "$D/tests/host/ModemStateTest.java"
java -cp "$O" com.sonymobile.customizationselector.ModemStateTest
rm -rf "$O"
