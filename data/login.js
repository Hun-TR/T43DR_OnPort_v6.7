// data/login.js - Düzeltilmiş versiyon

document.addEventListener('DOMContentLoaded', () => {
    const loginForm = document.getElementById('loginForm');
    const loginBtn = document.getElementById('loginBtn');
    const btnText = document.getElementById('btnText');
    const btnLoader = document.getElementById('btnLoader');

    console.log('🔐 Login sayfası yüklendi');

    // Mevcut token kontrolü - otomatik yönlendirme KALDIRILDI
    const existingToken = localStorage.getItem('sessionToken');
    if (existingToken) {
        console.log('⚠️ Zaten token var ama login sayfasındasınız');
        // Token'ı temizle çünkü login sayfasındaysak geçersiz olmalı
        localStorage.removeItem('sessionToken');
        console.log('🗑️ Eski token temizlendi');
    }

    if (loginForm) {
        loginForm.addEventListener('submit', async (e) => {
            e.preventDefault();
            console.log('📝 Login formu gönderiliyor...');

            // Butonu yükleme durumuna al
            loginBtn.disabled = true;
            btnText.style.display = 'none';
            btnLoader.style.display = 'inline-block';

            const formData = new FormData(loginForm);
            const username = formData.get('username');
            const password = formData.get('password');
            
            console.log('👤 Kullanıcı adı:', username);
            console.log('🔑 Şifre uzunluğu:', password ? password.length : 0);
            
            try {
                console.log('📡 Login API isteği gönderiliyor...');
                const response = await fetch('/login', {
                    method: 'POST',
                    body: new URLSearchParams(formData)
                });

                console.log('📡 Response status:', response.status);
                console.log('📡 Response ok:', response.ok);

                if (!response.ok) {
                    console.error('❌ HTTP Error:', response.status);
                    showError('Sunucu hatası: ' + response.status);
                    return;
                }

                const result = await response.json();
                console.log('📋 Login yanıtı:', result);

                if (result.success && result.token) {
                    console.log('✅ Login başarılı!');
                    console.log('🔑 Token alındı:', result.token.substring(0, 10) + '...');
                    
                    // Jetonu kaydet
                    localStorage.setItem('sessionToken', result.token);
                    console.log('💾 Token localStorage\'a kaydedildi');
                    
                    // Parola değiştirme kontrolü
                    if (result.mustChangePassword) {
                        console.log('🔒 Parola değiştirme zorunlu');
                        console.log('💬 Sebep:', result.reason || 'Belirtilmemiş');
                        
                        // Kullanıcıya mesaj göster
                        showMessage(result.reason || 'Parola değiştirme zorunlu', 'warning');
                        
                        // 1.5 saniye bekle sonra yönlendir
                        setTimeout(() => {
                            console.log('🔄 Password change sayfasına yönlendiriliyor...');
                            window.location.href = '/password_change.html';
                        }, 1500);
                        
                    } else {
                        console.log('🏠 Normal ana sayfaya yönlendiriliyor');
                        
                        // Başarı mesajı göster
                        showMessage('Giriş başarılı! Ana sayfaya yönlendiriliyorsunuz...', 'success');
                        
                        // 1 saniye bekle sonra yönlendir
                        setTimeout(() => {
                            window.location.href = '/';
                        }, 1000);
                    }
                } else {
                    console.error('❌ Login başarısız:', result.error);
                    showError(result.error || 'Bilinmeyen bir hata oluştu.');
                }
            } catch (error) {
                console.error('❌ Login request hatası:', error);
                showError('Sunucuya bağlanılamadı. Lütfen ağ bağlantınızı kontrol edin.');
            } finally {
                // Butonu tekrar aktif et
                loginBtn.disabled = false;
                btnText.style.display = 'inline';
                btnLoader.style.display = 'none';
            }
        });
    }

    function showError(message) {
        console.log('❌ Hata mesajı:', message);
        const errorContainer = document.getElementById('error-container');
        if (errorContainer) {
            errorContainer.innerHTML = `<div class="message error">${message}</div>`;
            setTimeout(() => {
                errorContainer.innerHTML = '';
            }, 5000);
        }
    }
    
    function showMessage(message, type = 'info') {
        console.log('💬 Mesaj:', type, message);
        const errorContainer = document.getElementById('error-container');
        if (errorContainer) {
            const className = type === 'warning' ? 'message warning' : 
                            type === 'success' ? 'message success' : 'message info';
            errorContainer.innerHTML = `<div class="${className}">${message}</div>`;
            
            // Success mesajları daha kısa süre göster
            const duration = type === 'success' ? 2000 : 4000;
            setTimeout(() => {
                errorContainer.innerHTML = '';
            }, duration);
        }
    }

    // Device IP'yi güncel tut
    fetch('/api/device-info', { method: 'GET' })
        .then(response => response.json())
        .then(data => {
            if (data.ip) {
                const deviceIPElement = document.getElementById('deviceIP');
                if (deviceIPElement) {
                    deviceIPElement.textContent = data.ip;
                }
            }
        })
        .catch(error => {
            console.log('Device info alınamadı:', error);
        });
});