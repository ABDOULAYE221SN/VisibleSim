#!/bin/bash
echo "=========================================="
echo "Lancement de MonApplication avec logs complets"
echo "=========================================="
./applicationsBin/MonApplication/MonApplication -c applicationsBin/MonApplication/config.xml -p 2>&1 | tee monapp_full_log.txt
echo ""
echo "=========================================="
echo "Logs sauvegardés dans monapp_full_log.txt"
echo "=========================================="
echo ""
echo "Résumé des mouvements:"
grep -E "(tryMoveToward|onMotionEnd|CIBLE ATTEINTE|DEBUT MOUVEMENT|RECONFIGURATION)" monapp_full_log.txt | head -50
