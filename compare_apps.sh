#!/bin/bash

echo "=========================================="
echo "Comparaison myMotionTest vs MonApplication"
echo "=========================================="
echo ""

echo "1. Test de myMotionTest (sans authentification)"
echo "----------------------------------------------"
read -p "Appuyez sur Entrée pour lancer myMotionTest..."
./applicationsBin/myMotionTest/myMotionTest -c applicationsBin/myMotionTest/config.xml -p

echo ""
echo ""
echo "2. Test de MonApplication (avec AKC-PM)"
echo "----------------------------------------------"
read -p "Appuyez sur Entrée pour lancer MonApplication..."
./applicationsBin/MonApplication/MonApplication -c applicationsBin/MonApplication/config.xml -p

echo ""
echo "=========================================="
echo "Comparaison terminée"
echo "=========================================="
