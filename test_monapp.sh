#!/bin/bash
echo "=========================================="
echo "Test de MonApplication"
echo "=========================================="
./applicationsBin/MonApplication/MonApplication -c applicationsBin/MonApplication/config.xml -p 2>&1 | tee test_output.log
echo ""
echo "=========================================="
echo "Log sauvegardé dans test_output.log"
echo "=========================================="
