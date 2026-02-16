#!/bin/bash
echo "Test MonApplication - Reconfiguration"
echo "======================================"
./applicationsBin/MonApplication/MonApplication -c applicationsBin/MonApplication/config.xml -p 2>&1 | grep -E "(Module|tryMoveToward|onMotionEnd|MOUVEMENT|CIBLE|RECONFIGURATION|DEBUT|authentifies)" | head -100
