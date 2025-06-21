#!/bin/bash

set -e
echo "🔐 Setting SMTP credentials..."
fly secrets set SMTP_USER=kushal.gore@gmail.com SMTP_PASS=qooszbmbfgoflmog

echo "🚀 Deploying app..."
fly deploy