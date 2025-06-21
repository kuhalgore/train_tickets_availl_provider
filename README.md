# HtmlJSON Server

A C++ API server using Crow + Mailio to send email from parsed JSON.

## 🔧 Build locally

```bash
mkdir build && cd build
cmake ..
make
export SMTP_USER=your_email@gmail.com
export SMTP_PASS=your_app_password
./HtmlJsonExtractor