#!/bin/bash
TEMPFILE=$(mktemp)
awk -v key="$1" -v val="$2" '$1==key {$3=val}1' $3 > $TEMPFILE
mv $TEMPFILE $3
