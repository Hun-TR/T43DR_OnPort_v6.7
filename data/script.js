// TEİAŞ EKLİM v5.2 - Tüm eksiklikler giderilmiş versiyon

document.addEventListener('DOMContentLoaded', () => {
    
    // --- 1. UYGULAMA DURUMU (STATE) VE AYARLAR ---
    const state = {
        token: localStorage.getItem('sessionToken') || null,
        logPaused: false,
        autoScroll: true,
        pollingIntervals: {
            status: null,
            logs: null,
            faults: null,
            notifications: null,
            systemInfo: null
        }
    };

    // Klavye navigasyonu için
document.addEventListener('keydown', function(e) {
    if (e.target.classList.contains('ip-part')) {
        // Backspace ile geri gitme
        if (e.key === 'Backspace' && e.target.value === '') {
            const part = parseInt(e.target.dataset.part);
            if (part > 1) {
                const prevInput = e.target.parentElement.querySelector(`.ip-part[data-part="${part - 1}"]`);
                if (prevInput) {
                    prevInput.focus();
                    prevInput.select();
                }
            }
        }
        // Sol ok ile geri gitme
        else if (e.key === 'ArrowLeft' && e.target.selectionStart === 0) {
            const part = parseInt(e.target.dataset.part);
            if (part > 1) {
                const prevInput = e.target.parentElement.querySelector(`.ip-part[data-part="${part - 1}"]`);
                if (prevInput) {
                    prevInput.focus();
                    prevInput.select();
                }
            }
        }
        // Sağ ok ile ileri gitme
        else if (e.key === 'ArrowRight' && e.target.selectionStart === e.target.value.length) {
            const part = parseInt(e.target.dataset.part);
            if (part < 4) {
                const nextInput = e.target.parentElement.querySelector(`.ip-part[data-part="${part + 1}"]`);
                if (nextInput) {
                    nextInput.focus();
                    nextInput.select();
                }
            }
        }
    }
});

// Her 5 dakikada bir session'ı canlı tut
let sessionKeepaliveInterval = null;

function startSessionKeepalive() {
    // Önceki interval varsa temizle
    if (sessionKeepaliveInterval) {
        clearInterval(sessionKeepaliveInterval);
    }
    
    // Her 5 dakikada bir basit bir API çağrısı yap
    sessionKeepaliveInterval = setInterval(async () => {
        try {
            const response = await fetch('/api/device-info', {
                headers: {
                    'Authorization': `Bearer ${state.token}`
                }
            });
            
            if (response.ok) {
                console.log('🔄 Session keepalive');
            }
        } catch (error) {
            // Sessizce başarısız ol
        }
    }, 300000); // 5 dakika
}

    // --- 2. SAYFA BAŞLATMA FONKSİYONLARI ---
    
    // Gösterge Paneli
function initDashboardPage() {
    console.log("Gösterge paneli başlatılıyor...");
    
    const updateStatus = () => {
        secureFetch('/api/status')
            .then(response => response && response.json())
            .then(data => data && updateDashboardUI(data))
            .catch(error => {
                console.error('Durum verileri alınamadı:', error);
                showMessage('Durum verileri alınamadı', 'error');
            });
    };
    
    updateStatus();
    state.pollingIntervals.status = setInterval(updateStatus, 5000);
    
    // LED Panel başlatma - BURAYA TAŞIYIN
    initLedPanelForDashboard();
}

// Yeni fonksiyon olarak ekleyin:
function initLedPanelForDashboard() {
    console.log("💡 LED Panel başlatılıyor (Dashboard için)...");
    
    // Sadece dashboard sayfasındayken çalışsın
    if (window.location.hash !== '#dashboard' && window.location.hash !== '') {
        return;
    }
    
    let ledAutoRefresh = true;
    
    async function updateLedStatus() {
        if (!ledAutoRefresh || window.location.hash !== '#dashboard' && window.location.hash !== '') {
            return;
        }
        
        try {
            const response = await secureFetch('/api/led/status');
            if (!response || !response.ok) {
                console.error('LED durumu alınamadı');
                return;
            }
            
            const data = await response.json();
            
            if (data.success && data.parsed && data.parsed.valid) {
                updateLedVisualsInDashboard(data.parsed);
                
                // Özet bilgileri güncelle
                const activeInputCount = document.getElementById('activeInputCount');
                if (activeInputCount) {
                    activeInputCount.textContent = data.parsed.activeInputs + '/8';
                }
                
                const activeOutputCount = document.getElementById('activeOutputCount');
                if (activeOutputCount) {
                    activeOutputCount.textContent = data.parsed.activeOutputs + '/8';
                }
                
                const statusText = document.getElementById('statusText');
                if (statusText) {
                    const now = new Date();
                    statusText.textContent = 'Son güncelleme: ' + now.toLocaleTimeString('tr-TR');
                }
            }
        } catch (error) {
            console.error('LED güncelleme hatası:', error);
        }
    }
    
    function updateLedVisualsInDashboard(parsedData) {
        // Input LED'leri - BLINK animasyonu ekle
        if (parsedData.inputs && Array.isArray(parsedData.inputs)) {
            for (let i = 0; i < 8; i++) {
                updateSingleLedInDashboard('led_I' + (i + 1), 'led_I' + (i + 1) + '_inner', parsedData.inputs[i], 'red', true);
            }
        }
        
        // Output LED'leri - BLINK animasyonu ekle
        if (parsedData.outputs && Array.isArray(parsedData.outputs)) {
            for (let i = 0; i < 8; i++) {
                updateSingleLedInDashboard('led_O' + (i + 1), 'led_O' + (i + 1) + '_inner', parsedData.outputs[i], 'red', true);
            }
        }
        
        // Alarm LED'leri - YENİ MANTIK
        if (parsedData.alarms) {
            const hasAnyAlarm = parsedData.alarms.general || 
                               parsedData.alarms.ntp || 
                               parsedData.alarms.rs232 || 
                               parsedData.alarms.dc1 || 
                               parsedData.alarms.dc2;
            
            // B1 - Sağlam LED (Her zaman yeşil ve blink)
            updateSafetyLed('led_B1', 'led_B1_inner');
            
            // B2 - Genel Alarm
            updateAlarmLed('led_B2', 'led_B2_inner', parsedData.alarms.general || false);
            
            // B3 - NTP Alarm  
            updateAlarmLed('led_B3', 'led_B3_inner', parsedData.alarms.ntp || false);
            
            // B4 - RS232 Alarm
            updateAlarmLed('led_B4', 'led_B4_inner', parsedData.alarms.rs232 || false);
            
            // B5 - DC1 Alarm
            updateAlarmLed('led_B5', 'led_B5_inner', parsedData.alarms.dc1 || false);
            
            // B6 - DC2 Alarm
            updateAlarmLed('led_B6', 'led_B6_inner', parsedData.alarms.dc2 || false);
        }
    }
    
    // Sağlam LED için özel fonksiyon (sürekli yeşil BLİNK - 1 sn yan, 1 sn sön)
    function updateSafetyLed(ledId, ledInnerId) {
        const ledOuter = document.getElementById(ledId);
        const ledInner = document.getElementById(ledInnerId);
        
        if (!ledOuter || !ledInner) return;
        
        // Mevcut animasyonları temizle
        while (ledInner.firstChild) {
            ledInner.removeChild(ledInner.firstChild);
        }
        while (ledOuter.firstChild) {
            ledOuter.removeChild(ledOuter.firstChild);
        }
        
        // Her zaman yeşil ve BLİNK
        ledOuter.setAttribute('fill', '#22c55e');
        ledOuter.setAttribute('stroke', '#16a34a');
        ledInner.setAttribute('fill', '#86efac');
        ledInner.setAttribute('opacity', '1');
        ledInner.setAttribute('filter', 'url(#ledGlow)');
        
        // TAMAMEN YAN-SÖN BLİNK ANİMASYONU
        const animateInner = document.createElementNS('http://www.w3.org/2000/svg', 'animate');
        animateInner.setAttribute('attributeName', 'opacity');
        animateInner.setAttribute('values', '1;1;0;0;1');
        animateInner.setAttribute('keyTimes', '0;0.49;0.5;0.99;1');
        animateInner.setAttribute('calcMode', 'discrete');
        animateInner.setAttribute('dur', '2s');
        animateInner.setAttribute('repeatCount', 'indefinite');
        ledInner.appendChild(animateInner);
        
        const animateOuter = document.createElementNS('http://www.w3.org/2000/svg', 'animate');
        animateOuter.setAttribute('attributeName', 'opacity');
        animateOuter.setAttribute('values', '1;1;0.2;0.2;1');
        animateOuter.setAttribute('keyTimes', '0;0.49;0.5;0.99;1');
        animateOuter.setAttribute('calcMode', 'discrete');
        animateOuter.setAttribute('dur', '2s');
        animateOuter.setAttribute('repeatCount', 'indefinite');
        ledOuter.appendChild(animateOuter);
    }
    
    // Input/Output LED'leri için güncelleme fonksiyonu
    function updateSingleLedInDashboard(ledId, ledInnerId, isOn, color, shouldBlink = false) {
        const ledOuter = document.getElementById(ledId);
        const ledInner = document.getElementById(ledInnerId);
        
        if (!ledOuter || !ledInner) return;
        
        // Mevcut animasyonları temizle
        while (ledInner.firstChild) {
            ledInner.removeChild(ledInner.firstChild);
        }
        while (ledOuter.firstChild) {
            ledOuter.removeChild(ledOuter.firstChild);
        }
        
        if (isOn) {
            ledOuter.setAttribute('fill', '#ef4444');
            ledOuter.setAttribute('stroke', '#dc2626');
            ledInner.setAttribute('fill', '#fca5a5');
            ledInner.setAttribute('opacity', '1');
            ledInner.setAttribute('filter', 'url(#ledGlow)');
            
            if (shouldBlink) {
                const animateInner = document.createElementNS('http://www.w3.org/2000/svg', 'animate');
                animateInner.setAttribute('attributeName', 'opacity');
                animateInner.setAttribute('values', '1;1;0;0;1');
                animateInner.setAttribute('keyTimes', '0;0.49;0.5;0.99;1');
                animateInner.setAttribute('calcMode', 'discrete');
                animateInner.setAttribute('dur', '2s');
                animateInner.setAttribute('repeatCount', 'indefinite');
                ledInner.appendChild(animateInner);
                
                const animateOuter = document.createElementNS('http://www.w3.org/2000/svg', 'animate');
                animateOuter.setAttribute('attributeName', 'opacity');
                animateOuter.setAttribute('values', '1;1;0.2;0.2;1');
                animateOuter.setAttribute('keyTimes', '0;0.49;0.5;0.99;1');
                animateOuter.setAttribute('calcMode', 'discrete');
                animateOuter.setAttribute('dur', '2s');
                animateOuter.setAttribute('repeatCount', 'indefinite');
                ledOuter.appendChild(animateOuter);
            }
        } else {
            ledOuter.setAttribute('fill', '#1a1a1a');
            ledOuter.setAttribute('stroke', '#2a2a2a');
            ledInner.setAttribute('fill', '#2a2a2a');
            ledInner.setAttribute('opacity', '0.3');
            ledInner.setAttribute('filter', 'none');
        }
    }
    
    // Alarm LED'leri için fonksiyon
    function updateAlarmLed(ledId, ledInnerId, hasAlarm) {
        const ledOuter = document.getElementById(ledId);
        const ledInner = document.getElementById(ledInnerId);
        
        if (!ledOuter || !ledInner) return;
        
        // Mevcut animasyonları temizle
        const existingAnimation = ledInner.querySelector('animate');
        if (existingAnimation) {
            existingAnimation.remove();
        }
        
        if (hasAlarm) {
            // ALARM VAR - Kırmızı ve SABİT (blink yok)
            ledOuter.setAttribute('fill', '#ef4444');
            ledOuter.setAttribute('stroke', '#dc2626');
            ledInner.setAttribute('fill', '#fca5a5');
            ledInner.style.opacity = '1';
            ledInner.setAttribute('filter', 'url(#ledGlow)');
            
        } else {
            // ALARM YOK - Yeşil ve sabit
            ledOuter.setAttribute('fill', '#22c55e');
            ledOuter.setAttribute('stroke', '#16a34a');
            ledInner.setAttribute('fill', '#86efac');
            ledInner.style.opacity = '1';
            ledInner.setAttribute('filter', 'url(#ledGlow)');
            // Animasyon yok, sabit yanıyor
        }
    }
    
    
    // Auto refresh toggle
    const autoRefreshToggle = document.getElementById('autoRefreshToggle');
    if (autoRefreshToggle) {
        autoRefreshToggle.addEventListener('change', (e) => {
            ledAutoRefresh = e.target.checked;
            console.log("LED otomatik yenileme: " + (ledAutoRefresh ? 'AÇIK' : 'KAPALI'));
            
            if (ledAutoRefresh) {
                updateLedStatus();
            }
        });
    }
    
    // Manuel yenileme butonu
    const refreshLedBtn = document.getElementById('refreshLedBtn');
    if (refreshLedBtn) {
        refreshLedBtn.addEventListener('click', () => {
            console.log("🔄 LED durumu manuel yenileniyor...");
            updateLedStatus();
        });
    }
    
    // İlk yükleme
    updateLedStatus();
    
    // 3 saniyede bir otomatik güncelle (sadece dashboard'ta)
    window.ledUpdateInterval = setInterval(() => {
        if (window.location.hash === '#dashboard' || window.location.hash === '') {
            updateLedStatus();
        } else {
            // Dashboard dışındaysa interval'i temizle
            clearInterval(window.ledUpdateInterval);
            window.ledUpdateInterval = null;
        }
    }, 3000);
}
   
// Network Ayarları Sayfası - SADECE STATİK IP VERSİYONU
function initNetworkPage() {
    console.log("🌐 Network sayfası başlatılıyor (Statik IP)...");
    
    const form = document.getElementById('networkForm');
    const refreshNetworkBtn = document.getElementById('refreshNetworkBtn');
    const resetNetworkBtn = document.getElementById('resetNetworkBtn');
    
    if (!form) {
        console.error('❌ Network form bulunamadı!');
        return;
    }
    
    // Mevcut network durumunu yükle
    loadNetworkStatus();
    
    // IP validation helper
    function validateIPAddress(ip) {
        const ipRegex = /^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/;
        return ipRegex.test(ip);
    }
    
    // Real-time IP validation
    const ipInputs = ['staticIP', 'gateway', 'subnet', 'dns1', 'dns2'];
    ipInputs.forEach(inputId => {
        const input = document.getElementById(inputId);
        if (input) {
            input.addEventListener('blur', function() {
                if (this.value) {
                    if (!validateIPAddress(this.value)) {
                        this.style.borderColor = 'var(--error)';
                        this.style.backgroundColor = 'rgba(245, 101, 101, 0.1)';
                        showMessage(`Geçersiz IP adresi: ${this.value}`, 'error');
                    } else {
                        this.style.borderColor = '';
                        this.style.backgroundColor = '';
                        
                        // Validation indicator güncelle
                        updateValidationIndicator(inputId, true);
                    }
                }
            });
            
            // Enter tuşu ile sonraki alana geç
            input.addEventListener('keypress', function(e) {
                if (e.key === 'Enter') {
                    e.preventDefault();
                    const currentIndex = ipInputs.indexOf(inputId);
                    if (currentIndex < ipInputs.length - 1) {
                        const nextInput = document.getElementById(ipInputs[currentIndex + 1]);
                        if (nextInput) nextInput.focus();
                    } else {
                        // Son alan, form submit
                        form.dispatchEvent(new Event('submit'));
                    }
                }
            });
        }
    });
    
    // Validation indicator güncelleme
    function updateValidationIndicator(fieldId, isValid) {
        const validationDiv = document.getElementById('ipValidation');
        if (validationDiv) {
            validationDiv.style.display = 'block';
            
            let itemId = '';
            let text = '';
            
            switch(fieldId) {
                case 'staticIP':
                    itemId = 'ipValidItem';
                    text = 'IP Adresi';
                    break;
                case 'gateway':
                    itemId = 'gatewayValidItem';
                    text = 'Gateway';
                    break;
                case 'subnet':
                    itemId = 'subnetValidItem';
                    text = 'Subnet';
                    break;
                case 'dns1':
                case 'dns2':
                    itemId = 'dnsValidItem';
                    text = 'DNS';
                    break;
            }
            
            const item = document.getElementById(itemId);
            if (item) {
                const icon = item.querySelector('.validation-icon');
                const textEl = item.querySelector('.validation-text');
                
                if (isValid) {
                    icon.textContent = '✅';
                    textEl.textContent = `${text}: Geçerli`;
                    item.style.color = 'var(--success)';
                } else {
                    icon.textContent = '❌';
                    textEl.textContent = `${text}: Geçersiz`;
                    item.style.color = 'var(--error)';
                }
            }
        }
    }
    
    // Form validation
    function validateNetworkForm() {
        const requiredFields = ['staticIP', 'gateway', 'subnet', 'dns1'];
        
        for (const fieldId of requiredFields) {
            const field = document.getElementById(fieldId);
            if (!field || !field.value.trim()) {
                showMessage(`${fieldId} alanı zorunludur`, 'error');
                if (field) {
                    field.style.borderColor = 'var(--error)';
                    field.focus();
                }
                return false;
            }
            
            if (!validateIPAddress(field.value.trim())) {
                showMessage(`Geçersiz IP adresi: ${field.value}`, 'error');
                field.style.borderColor = 'var(--error)';
                field.focus();
                updateValidationIndicator(fieldId, false);
                return false;
            }
        }
        
        // DNS2 opsiyonel ama girilmişse valid olmalı
        const dns2 = document.getElementById('dns2');
        if (dns2 && dns2.value.trim() && !validateIPAddress(dns2.value.trim())) {
            showMessage(`Geçersiz DNS2 adresi: ${dns2.value}`, 'error');
            dns2.style.borderColor = 'var(--error)';
            dns2.focus();
            updateValidationIndicator('dns2', false);
            return false;
        }
        
        return true;
    }
    
    // Form gönderim handler'ı
    form.addEventListener('submit', async function(e) {
        e.preventDefault();
        
        if (!validateNetworkForm()) {
            return;
        }
        
        const saveBtn = document.getElementById('saveNetworkBtn');
        const btnText = saveBtn?.querySelector('.btn-text');
        const btnLoader = saveBtn?.querySelector('.btn-loader');
        
        // Loading state
        if (saveBtn) saveBtn.disabled = true;
        if (btnText) btnText.style.display = 'none';
        if (btnLoader) btnLoader.style.display = 'inline-block';
        
        const formData = new FormData(form);
        
        // Debug: Form verilerini logla
        console.log('📤 Network form verileri gönderiliyor (Statik IP)...');
        for (let [key, value] of formData.entries()) {
            console.log(`${key}: ${value}`);
        }
        
        try {
            const response = await secureFetch('/api/network', {
                method: 'POST',
                body: new URLSearchParams(formData)
            });
            
            if (response && response.ok) {
                const result = await response.json();
                showMessage(result.message || 'Network ayarları kaydedildi! Cihaz yeniden başlatılıyor...', 'success');
                
                // Countdown timer göster
                let countdown = 10;
                const countdownInterval = setInterval(() => {
                    showMessage(`Cihaz ${countdown} saniye içinde yeniden başlatılıyor...`, 'warning');
                    countdown--;
                    
                    if (countdown < 0) {
                        clearInterval(countdownInterval);
                        // Yeni IP ile yönlendirme
                        const newIP = formData.get('staticIP');
                        if (newIP) {
                            window.location.href = `http://${newIP}`;
                        } else {
                            window.location.href = '/';
                        }
                    }
                }, 1000);
                
            } else {
                const errorText = response ? await response.text() : 'Ağ hatası';
                showMessage('Network ayarları kaydedilemedi: ' + errorText, 'error');
            }
        } catch (error) {
            console.error('❌ Network kayıt hatası:', error);
            showMessage('Network ayarları kaydedilirken bir hata oluştu', 'error');
        } finally {
            // Reset loading state
            if (saveBtn) saveBtn.disabled = false;
            if (btnText) btnText.style.display = 'inline';
            if (btnLoader) btnLoader.style.display = 'none';
        }
    });
    
    // Preset butonları için event listeners
    document.querySelectorAll('.preset-network-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            const ip = this.dataset.ip;
            const gw = this.dataset.gw;
            const subnet = this.dataset.subnet;
            const dns = this.dataset.dns;
            
            // Değerleri doldur
            updateElement('staticIP', ip);
            updateElement('gateway', gw);
            updateElement('subnet', subnet);
            updateElement('dns1', dns);
            
            // Validation indicator'ları güncelle
            ['staticIP', 'gateway', 'subnet', 'dns1'].forEach(fieldId => {
                updateValidationIndicator(fieldId, true);
            });
            
            showMessage(`✅ ${this.textContent.trim()} ayarları yüklendi`, 'success');
        });
    });
    
    // Varsayılanlara sıfırla butonu
    if (resetNetworkBtn) {
        resetNetworkBtn.addEventListener('click', function() {
            // Varsayılan değerler
            const defaults = {
                staticIP: '192.168.1.160',
                gateway: '192.168.1.1',
                subnet: '255.255.255.0',
                dns1: '8.8.8.8',
                dns2: '8.8.4.4'
            };
            
            // Form alanlarını doldur
            Object.keys(defaults).forEach(fieldId => {
                updateElement(fieldId, defaults[fieldId]);
                if (fieldId !== 'dns2') { // DNS2 opsiyonel
                    updateValidationIndicator(fieldId, true);
                }
            });
            
            showMessage('✅ Varsayılan değerler yüklendi', 'info');
        });
    }
    
    // Yenile butonu
    if (refreshNetworkBtn) {
        refreshNetworkBtn.addEventListener('click', function() {
            showMessage('Ağ durumu yenileniyor...', 'info');
            loadNetworkStatus();
        });
    }
    
    // Network test butonu
    const networkTestBtn = document.getElementById('networkTestBtn');
    if (networkTestBtn) {
        networkTestBtn.addEventListener('click', async function() {
            showMessage('Network bağlantısı test ediliyor...', 'info');
            
            try {
                const response = await secureFetch('/api/network');
                if (response && response.ok) {
                    const data = await response.json();
                    
                    const resultsDiv = document.getElementById('networkTestResults');
                    const contentDiv = document.getElementById('testResultsContent');
                    
                    if (resultsDiv && contentDiv) {
                        let resultHTML = `
                            <div class="test-item">
                                <strong>Ethernet Kablosu:</strong> 
                                <span class="${data.linkUp ? 'success' : 'error'}">${data.linkUp ? '✅ Bağlı' : '❌ Bağlı Değil'}</span>
                            </div>
                            <div class="test-item">
                                <strong>Link Hızı:</strong> 
                                <span>${data.linkSpeed} Mbps</span>
                            </div>
                            <div class="test-item">
                                <strong>Duplex:</strong> 
                                <span>${data.fullDuplex ? 'Full Duplex' : 'Half Duplex'}</span>
                            </div>
                        `;
                        
                        contentDiv.innerHTML = resultHTML;
                        resultsDiv.style.display = 'block';
                    }
                    
                    if (data.linkUp) {
                        showMessage('✅ Network bağlantısı başarılı', 'success');
                    } else {
                        showMessage('❌ Network bağlantısı yok', 'error');
                    }
                }
            } catch (error) {
                showMessage('Network testi başarısız', 'error');
            }
        });
    }
    
    console.log('✅ Network sayfası hazır (Statik IP)');
}

// Network durumu yükleme fonksiyonu
async function loadNetworkStatus() {
    try {
        console.log('🔄 Network durumu yükleniyor...');
        
        const response = await secureFetch('/api/network');
        if (response && response.ok) {
            const data = await response.json();
            console.log('📊 Network verisi alındı:', data);
            
            // Durum göstergelerini güncelle
            updateElement('ethStatus', data.linkUp ? 'Bağlı' : 'Bağlı Değil');
            updateElement('currentIP', data.ip || 'Bilinmiyor');
            updateElement('macAddress', data.mac || 'Bilinmiyor');
            updateElement('linkSpeed', (data.linkSpeed || 0) + ' Mbps');
            updateElement('currentGateway', data.gateway || 'Bilinmiyor');
            updateElement('currentDNS', data.dns1 || 'Bilinmiyor');
            
            // Status badge rengini güncelle
            const ethStatusEl = document.getElementById('ethStatus');
            if (ethStatusEl) {
                ethStatusEl.className = `status-value ${data.linkUp ? 'online' : 'offline'}`;
            }
            
            // Form değerlerini doldur (her zaman statik IP değerleri)
            updateElement('staticIP', data.ip);
            updateElement('gateway', data.gateway);
            updateElement('subnet', data.subnet);
            updateElement('dns1', data.dns1);
            updateElement('dns2', data.dns2 || '');
            
            // Gelişmiş bilgiler
            updateElement('hostname', 'teias-eklim');
            updateElement('linkStatus', data.linkUp ? 'Aktif' : 'Pasif');
            updateElement('duplexMode', data.fullDuplex ? 'Full Duplex' : 'Half Duplex');
            updateElement('mtuSize', '1500');
            updateElement('lastNetworkChange', new Date().toLocaleTimeString());
            
            console.log('✅ Network durumu yüklendi');
        } else {
            console.error('❌ Network durumu alınamadı');
            showMessage('Network bilgileri yüklenemedi', 'error');
        }
    } catch (error) {
        console.error('❌ Network durumu yükleme hatası:', error);
        showMessage('Network durumu yüklenirken hata oluştu', 'error');
    }
}

// updateElement fonksiyonu güvenli versiyon
function updateElement(id, value) {
    const element = document.getElementById(id);
    if (element) {
        if (element.tagName === 'INPUT') {
            element.value = value || '';
        } else {
            element.textContent = value || '';
        }
    }
}

// DateTime sayfası başlatma fonksiyonu
function initDateTimePage() {
    console.log('🕒 DateTime sayfası başlatılıyor...');
    
    // Sayfa elementleri
    const getDateTimeBtn = document.getElementById('getDateTimeBtn');
    const refreshDateTimeBtn = document.getElementById('refreshDateTimeBtn');
    const datetimeForm = document.getElementById('datetimeForm');
    const setCurrentBtn = document.getElementById('setCurrentBtn');
    const resetFormBtn = document.getElementById('resetFormBtn');
    
    if (!getDateTimeBtn) {
        console.error('DateTime sayfa elementleri bulunamadı');
        return;
    }
    
    // İlk yüklemede datetime bilgisini çek
    loadDateTimeStatus();
    
    // Event listener'ları ekle
    getDateTimeBtn.addEventListener('click', fetchDateTimeFromDsPIC);
    refreshDateTimeBtn.addEventListener('click', loadDateTimeStatus);
    datetimeForm.addEventListener('submit', handleSetDateTime);
    setCurrentBtn.addEventListener('click', setCurrentDateTime);
    resetFormBtn.addEventListener('click', resetDateTimeForm);
    
    console.log('✅ DateTime sayfası hazır');
}

// DateTime durumunu yükle
async function loadDateTimeStatus() {
    try {
        console.log('📡 DateTime durumu yükleniyor...');
        
        const response = await secureFetch('/api/datetime');
        if (response && response.ok) {
            const data = await response.json();
            
            // UI'ı güncelle
            updateElement('currentDate', data.date || '--/--/--');
            updateElement('currentTime', data.time || '--:--:--');
            updateElement('lastUpdate', data.lastUpdate || 'Henüz çekilmedi');
            updateElement('rawData', data.rawData || 'Bekleniyor...');
            
            console.log('✅ DateTime durumu yüklendi:', data);
        } else {
            console.error('❌ DateTime durumu yüklenemedi');
            showMessage('DateTime durumu yüklenemedi', 'error');
        }
    } catch (error) {
        console.error('DateTime durumu yükleme hatası:', error);
        showMessage('DateTime durumu yüklenirken hata oluştu', 'error');
    }
}

// dsPIC'ten datetime bilgisi çek
async function fetchDateTimeFromDsPIC() {
    const getBtn = document.getElementById('getDateTimeBtn');
    const btnText = getBtn.querySelector('.btn-text');
    const btnIcon = getBtn.querySelector('.btn-icon');
    
    // Loading state
    getBtn.disabled = true;
    btnIcon.textContent = '⏳';
    btnText.textContent = 'Çekiliyor...';
    
    try {
        console.log('📡 dsPIC\'ten datetime çekiliyor...');
        
        const response = await secureFetch('/api/datetime/fetch', {
            method: 'POST'
        });
        
        if (response && response.ok) {
            const data = await response.json();
            
            if (data.success) {
                // UI'ı güncelle
                updateElement('currentDate', data.date);
                updateElement('currentTime', data.time);
                updateElement('lastUpdate', 'Az önce');
                updateElement('rawData', data.rawData);
                
                showMessage('✅ Tarih-saat bilgisi başarıyla güncellendi', 'success');
                console.log('✅ DateTime çekildi:', data);
            } else {
                showMessage('❌ ' + (data.message || 'Tarih-saat bilgisi alınamadı'), 'error');
                console.error('DateTime fetch başarısız:', data);
            }
        } else {
            showMessage('❌ Sunucu hatası', 'error');
        }
    } catch (error) {
        console.error('DateTime fetch hatası:', error);
        showMessage('❌ DateTime bilgisi çekilirken hata oluştu', 'error');
    } finally {
        // Reset loading state
        getBtn.disabled = false;
        btnIcon.textContent = '📥';
        btnText.textContent = 'Sistem Saatini Çek';
    }
}

// DateTime ayarlama formu
async function handleSetDateTime(e) {
    e.preventDefault();
    
    const formData = new FormData(e.target);
    const manualDate = formData.get('manualDate');
    const manualTime = formData.get('manualTime');
    
    if (!manualDate || !manualTime) {
        showMessage('❌ Tarih ve saat alanları doldurulmalıdır', 'error');
        return;
    }
    
    const setBtn = document.getElementById('setDateTimeBtn');
    const btnText = setBtn.querySelector('.btn-text');
    const btnLoader = setBtn.querySelector('.btn-loader');
    
    // Loading state
    setBtn.disabled = true;
    btnText.style.display = 'none';
    btnLoader.style.display = 'inline-block';
    
    try {
        console.log('📤 DateTime ayarlanıyor:', manualDate, manualTime);
        
        const params = new URLSearchParams();
        params.append('manualDate', manualDate);
        params.append('manualTime', manualTime);
        
        const response = await secureFetch('/api/datetime/set', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded'
            },
            body: params.toString()
        });
        
        if (response && response.ok) {
            const data = await response.json();
            
            if (data.success) {
                showMessage('✅ Tarih-saat başarıyla ayarlandı', 'success');
                console.log('✅ DateTime ayarlandı:', data);
                
                // Formu temizle ve durumu güncelle
                resetDateTimeForm();
                setTimeout(() => {
                    loadDateTimeStatus();
                }, 1000);
            } else {
                showMessage('❌ ' + (data.message || 'Tarih-saat ayarlanamadı'), 'error');
                console.error('DateTime set başarısız:', data);
            }
        } else {
            const errorText = await response.text();
            console.error('Sunucu hatası detayı:', errorText);
            showMessage('❌ Sunucu hatası: ' + (errorText || 'Bilinmeyen hata'), 'error');
        }
    } catch (error) {
        console.error('DateTime set hatası:', error);
        showMessage('❌ Tarih-saat ayarlanırken hata oluştu', 'error');
    } finally {
        // Reset loading state
        setBtn.disabled = false;
        btnText.style.display = 'inline';
        btnLoader.style.display = 'none';
    }
}

// Şimdiki zamanı form alanlarına doldur
function setCurrentDateTime() {
    const now = new Date();
    
    // Tarih formatı: YYYY-MM-DD
    const year = now.getFullYear();
    const month = String(now.getMonth() + 1).padStart(2, '0');
    const day = String(now.getDate()).padStart(2, '0');
    const dateStr = `${year}-${month}-${day}`;
    
    // Saat formatı: HH:MM:SS
    const hours = String(now.getHours()).padStart(2, '0');
    const minutes = String(now.getMinutes()).padStart(2, '0');
    const seconds = String(now.getSeconds()).padStart(2, '0');
    const timeStr = `${hours}:${minutes}:${seconds}`;
    
    // Form alanlarını doldur
    document.getElementById('manualDate').value = dateStr;
    document.getElementById('manualTime').value = timeStr;
    
    showMessage('✅ Şimdiki tarih ve saat form alanlarına yerleştirildi', 'info');
    console.log('🕐 Form alanları dolduruldu:', dateStr, timeStr);
}

// Formu temizle
function resetDateTimeForm() {
    const form = document.getElementById('datetimeForm');
    
    if (form) {
        form.reset();
    }
    
    showMessage('✅ Form temizlendi', 'info');
}

// HTML escape helper
function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

    // System Info Sayfası - YENİ
    function initSystemInfoPage() {
        const updateSystemInfo = async () => {
            try {
                const response = await secureFetch('/api/system-info');
                if (response && response.ok) {
                    const data = await response.json();
                    
                    // Hardware bilgileri
                    updateElement('chipModel', data.hardware.chip);
                    updateElement('coreCount', data.hardware.cores);
                    updateElement('cpuFreq', data.hardware.frequency + ' MHz');
                    updateElement('chipRevision', data.hardware.revision);
                    updateElement('flashSize', formatBytes(data.hardware.flashSize));
                    
                    // Memory bilgileri
                    updateElement('totalHeap', formatBytes(data.memory.totalHeap));
                    updateElement('usedHeap', formatBytes(data.memory.usedHeap));
                    updateElement('freeHeap', formatBytes(data.memory.freeHeap));
                    updateElement('minFreeHeap', formatBytes(data.memory.minFreeHeap));
                    
                    const usagePercent = Math.round((data.memory.usedHeap / data.memory.totalHeap) * 100);
                    updateElement('ramUsageBar', '', usagePercent);
                    updateElement('ramUsagePercent', usagePercent + '%');
                    document.getElementById('ramUsageBar').style.width = usagePercent + '%';
                    
                    // Software bilgileri
                    updateElement('firmwareVersion', 'v' + data.software.version);
                    updateElement('sdkVersion', data.software.sdk);
                    updateElement('buildDate', data.software.buildDate);
                    updateElement('uptime', formatUptime(data.software.uptime));
                    
                    // UART istatistikleri
                    updateElement('uartTxCount', data.uart.txCount);
                    updateElement('uartRxCount', data.uart.rxCount);
                    updateElement('uartErrorCount', data.uart.errors);
                    updateElement('uartSuccessRate', data.uart.successRate.toFixed(1) + '%');
                    updateElement('currentBaud', data.uart.baudRate);
                    
                    // Dosya sistemi
                    updateElement('totalSpace', formatBytes(data.filesystem.total));
                    updateElement('usedSpace', formatBytes(data.filesystem.used));
                    updateElement('freeSpace', formatBytes(data.filesystem.free));
                }
            } catch (error) {
                console.error('System info hatası:', error);
                showMessage('Sistem bilgileri alınamadı', 'error');
            }
        };

        updateSystemInfo();
        state.pollingIntervals.systemInfo = setInterval(updateSystemInfo, 10000);

        // Yenile butonu
        document.getElementById('refreshBtn')?.addEventListener('click', updateSystemInfo);

        // Yeniden başlat butonu
        document.getElementById('rebootBtn')?.addEventListener('click', async () => {
            if (confirm('Sistemi yeniden başlatmak istediğinize emin misiniz?')) {
                const response = await secureFetch('/api/system/reboot', { method: 'POST' });
                if (response && response.ok) {
                    showMessage('Sistem yeniden başlatılıyor...', 'warning');
                    setTimeout(() => {
                        window.location.href = '/';
                    }, 3000);
                }
            }
        });
    }

    // Hesap Ayarları
    function initAccountPage() {
        const form = document.getElementById('accountForm');
        if (!form) return;

        secureFetch('/api/settings').then(r => r && r.json()).then(settings => {
            if (settings) {
                form.querySelector('#deviceName').value = settings.deviceName || '';
                form.querySelector('#tmName').value = settings.tmName || '';
                form.querySelector('#username').value = settings.username || '';
            }
        }).catch(error => {
            console.error('Ayarlar yüklenemedi:', error);
            showMessage('Ayarlar yüklenemedi', 'error');
        });

        form.addEventListener('submit', async (e) => {
            e.preventDefault();
            try {
                const response = await secureFetch('/api/settings', {
                    method: 'POST',
                    body: new URLSearchParams(new FormData(form))
                });
                showMessage(response && response.ok ? 'Ayarlar başarıyla kaydedildi.' : 'Ayarlar kaydedilirken bir hata oluştu.', response && response.ok ? 'success' : 'error');
            } catch (error) {
                console.error('Ayar kayıt hatası:', error);
                showMessage('Bir hata oluştu', 'error');
            }
        });
    }

    // NTP Ayarları
    // Global fonksiyon - window nesnesine ekle ki HTML'den çağrılabilsin
window.moveToNext = function(input, nextPart, isSecondary = false) {
    const value = input.value;
    
    // Sadece sayı girişine izin ver ve temizle
    const numericValue = value.replace(/[^0-9]/g, '');
    input.value = numericValue;
    
    // 255'i aşmasını engelle
    if (parseInt(numericValue) > 255) {
        input.value = '255';
    }
    
    // Otomatik geçiş koşulları
    const shouldMoveNext = (input.value.length === 3) || 
                          (input.value === '255') || 
                          (input.value.length === 2 && parseInt(input.value) > 25);
    
    if (shouldMoveNext && nextPart <= 4) {
        const nextInput = getNextIPInput(input, nextPart, isSecondary);
        if (nextInput) {
            setTimeout(() => {
                nextInput.focus();
                nextInput.select();
            }, 10);
        }
    }
    
    // Hidden input'u güncelle
    updateHiddenIPInput(isSecondary);
    
    // Container'ı validate et
    validateIPContainer(input.closest('.ip-input-container'));
};

function getNextIPInput(currentInput, nextPart, isSecondary) {
    if (isSecondary) {
        return document.getElementById(`ntp2-part${nextPart}`);
    } else {
        const container = currentInput.closest('.ip-input-container');
        return container ? container.querySelector(`.ip-part[data-part="${nextPart}"]`) : null;
    }
}

function getPrevIPInput(currentInput, currentPart, isSecondary) {
    if (currentPart <= 1) return null;
    
    if (isSecondary) {
        return document.getElementById(`ntp2-part${currentPart - 1}`);
    } else {
        const container = currentInput.closest('.ip-input-container');
        return container ? container.querySelector(`.ip-part[data-part="${currentPart - 1}"]`) : null;
    }
}

function updateHiddenIPInput(isSecondary = false) {
    const hiddenId = isSecondary ? 'ntpServer2' : 'ntpServer1';
    const hiddenInput = document.getElementById(hiddenId);
    
    if (!hiddenInput) return;
    
    let parts = [];
    
    if (isSecondary) {
        // İkincil NTP için ID'leri kullan
        for (let i = 1; i <= 4; i++) {
            const input = document.getElementById(`ntp2-part${i}`);
            const value = input ? (input.value || '0') : '0';
            parts.push(value);
        }
    } else {
        // Birincil NTP için container'dan seç
        const container = document.querySelector('.ip-input-container:not(:has(#ntp2-part1))');
        if (container) {
            const inputs = container.querySelectorAll('.ip-part');
            inputs.forEach(input => {
                const value = input.value || '0';
                parts.push(value);
            });
        } else {
            parts = ['0', '0', '0', '0'];
        }
    }
    
    const ip = parts.join('.');
    hiddenInput.value = ip;
    
    console.log(`${isSecondary ? 'NTP2' : 'NTP1'} güncellendi:`, ip);
}

function validateIPContainer(container) {
    if (!container) return false;
    
    const inputs = container.querySelectorAll('.ip-part');
    let isValid = true;
    let isEmpty = true;
    
    inputs.forEach(input => {
        const value = input.value.trim();
        if (value !== '' && value !== '0') {
            isEmpty = false;
        }
        
        if (value !== '') {
            const num = parseInt(value);
            if (isNaN(num) || num < 0 || num > 255) {
                isValid = false;
            }
        }
    });
    
    // CSS class'larını güncelle
    container.classList.remove('valid', 'invalid', 'empty');
    
    if (isEmpty) {
        container.classList.add('empty');
        return false;
    } else if (isValid) {
        container.classList.add('valid');
        return true;
    } else {
        container.classList.add('invalid');
        return false;
    }
}

function validateIPFormat(ip) {
    if (!ip || ip.trim() === '' || ip === '0.0.0.0') return false;
    
    const parts = ip.split('.');
    if (parts.length !== 4) return false;
    
    return parts.every(part => {
        const num = parseInt(part);
        return !isNaN(num) && num >= 0 && num <= 255 && part === num.toString();
    });
}

function validateNTPForm() {
    const ntp1 = document.getElementById('ntpServer1').value;
    const ntp2 = document.getElementById('ntpServer2').value;
    
    console.log('NTP Form Validation:', { ntp1, ntp2 });
    
    // Birincil NTP zorunlu kontrol
    if (!validateIPFormat(ntp1)) {
        showMessage('Lütfen geçerli bir birincil NTP IP adresi girin. Örnek: 192.168.1.1', 'error');
        
        // İlk container'a focus et
        const firstContainer = document.querySelector('.ip-input-container:not(:has(#ntp2-part1))');
        if (firstContainer) {
            const firstInput = firstContainer.querySelector('.ip-part');
            if (firstInput) firstInput.focus();
            firstContainer.classList.add('invalid');
        }
        return false;
    }
    
    // İkincil NTP opsiyonel ama girilmişse geçerli olmalı
    if (ntp2 && ntp2 !== '0.0.0.0' && !validateIPFormat(ntp2)) {
        showMessage('İkincil NTP IP adresi geçersiz. Boş bırakabilir veya geçerli IP girebilirsiniz.', 'error');
        
        // İkinci container'a focus et
        const secondContainer = document.querySelector('.ip-input-container:has(#ntp2-part1)');
        if (secondContainer) {
            const firstInput = secondContainer.querySelector('.ip-part');
            if (firstInput) firstInput.focus();
            secondContainer.classList.add('invalid');
        }
        return false;
    }
    
    return true;
}

function loadCurrentNTPToInputs(server1, server2) {
    console.log('NTP değerleri yükleniyor:', { server1, server2 });
    
    // Birincil NTP yükle
    if (server1 && validateIPFormat(server1)) {
        const parts = server1.split('.');
        const container = document.querySelector('.ip-input-container:not(:has(#ntp2-part1))');
        if (container) {
            const inputs = container.querySelectorAll('.ip-part');
            parts.forEach((part, index) => {
                if (inputs[index]) {
                    inputs[index].value = part;
                }
            });
            updateHiddenIPInput(false);
            validateIPContainer(container);
        }
    }
    
    // İkincil NTP yükle
    if (server2 && validateIPFormat(server2)) {
        const parts = server2.split('.');
        for (let i = 1; i <= 4; i++) {
            const input = document.getElementById(`ntp2-part${i}`);
            if (input && parts[i-1]) {
                input.value = parts[i-1];
            }
        }
        updateHiddenIPInput(true);
        const container2 = document.querySelector('.ip-input-container:has(#ntp2-part1)');
        validateIPContainer(container2);
    }
}

function setupIPInputKeyboardHandlers() {
    document.addEventListener('keydown', function(e) {
        if (!e.target.classList.contains('ip-part')) return;
        
        const currentInput = e.target;
        const currentPart = parseInt(currentInput.dataset.part);
        const isSecondary = currentInput.id && currentInput.id.startsWith('ntp2-');
        
        switch(e.key) {
            case 'Backspace':
                if (currentInput.value === '' && currentInput.selectionStart === 0) {
                    e.preventDefault();
                    const prevInput = getPrevIPInput(currentInput, currentPart, isSecondary);
                    if (prevInput) {
                        prevInput.focus();
                        prevInput.setSelectionRange(prevInput.value.length, prevInput.value.length);
                    }
                }
                break;
                
            case 'ArrowLeft':
                if (currentInput.selectionStart === 0) {
                    e.preventDefault();
                    const prevInput = getPrevIPInput(currentInput, currentPart, isSecondary);
                    if (prevInput) {
                        prevInput.focus();
                        prevInput.setSelectionRange(prevInput.value.length, prevInput.value.length);
                    }
                }
                break;
                
            case 'ArrowRight':
                if (currentInput.selectionStart === currentInput.value.length) {
                    e.preventDefault();
                    const nextInput = getNextIPInput(currentInput, currentPart + 1, isSecondary);
                    if (nextInput) {
                        nextInput.focus();
                        nextInput.setSelectionRange(0, 0);
                    }
                }
                break;
                
            case '.':
            case 'Period':
                e.preventDefault();
                const nextInput = getNextIPInput(currentInput, currentPart + 1, isSecondary);
                if (nextInput) {
                    nextInput.focus();
                    nextInput.select();
                }
                break;
                
            case 'Tab':
                // Tab normal davranışını korur, müdahale etme
                break;
                
            default:
                // Sadece sayısal girişe izin ver
                if (!/[0-9]/.test(e.key) && 
                    !['Backspace', 'Delete', 'Tab', 'ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(e.key) &&
                    !e.ctrlKey && !e.metaKey) {
                    e.preventDefault();
                }
        }
    });
    
    // Input change olayları
    document.addEventListener('input', function(e) {
        if (e.target.classList.contains('ip-part')) {
            const isSecondary = e.target.id && e.target.id.startsWith('ntp2-');
            
            // Değeri güncelle
            setTimeout(() => {
                updateHiddenIPInput(isSecondary);
                validateIPContainer(e.target.closest('.ip-input-container'));
            }, 10);
        }
    });
}

function addPresetServerButtons() {
    const form = document.getElementById('ntpForm');
    if (!form) return;
    
    const firstSection = form.querySelector('.settings-section');
    if (!firstSection || firstSection.querySelector('.preset-servers')) return; // Zaten eklenmişse çık
    
    const presetHTML = `
        <div class="preset-servers">
            <h4>🚀 Hızlı NTP Sunucu Seçenekleri</h4>
            <div class="preset-buttons">
                <button type="button" class="preset-btn" data-ip="192.168.1.1" title="Yerel Router/Modem">
                    🏠 Router (192.168.1.1)
                </button>
                <button type="button" class="preset-btn" data-ip="8.8.8.8" title="Google Public DNS">
                    🌐 Google (8.8.8.8)
                </button>
                <button type="button" class="preset-btn" data-ip="1.1.1.1" title="Cloudflare DNS">
                    ⚡ Cloudflare (1.1.1.1)
                </button>
                <button type="button" class="preset-btn" data-ip="208.67.222.222" title="OpenDNS">
                    🔒 OpenDNS (208.67.222.222)
                </button>
            </div>
        </div>
    `;
    
    firstSection.insertAdjacentHTML('beforeend', presetHTML);
    
    // Event listener'ları ekle
    form.querySelectorAll('.preset-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            const ip = this.dataset.ip;
            const parts = ip.split('.');
            
            // Birincil NTP'ye yükle
            const container = document.querySelector('.ip-input-container:not(:has(#ntp2-part1))');
            if (container) {
                const inputs = container.querySelectorAll('.ip-part');
                parts.forEach((part, index) => {
                    if (inputs[index]) {
                        inputs[index].value = part;
                        
                        // Güzel bir animasyon efekti
                        inputs[index].style.background = 'rgba(72, 187, 120, 0.3)';
                        setTimeout(() => {
                            inputs[index].style.background = '';
                        }, 500);
                    }
                });
                
                updateHiddenIPInput(false);
                validateIPContainer(container);
                
                showMessage(`✅ Birincil NTP sunucu: ${ip} seçildi`, 'success');
            }
        });
    });
}

// Manuel senkronizasyon butonu
const syncNtpBtn = document.getElementById('syncNtpBtn');
if (syncNtpBtn) {
    syncNtpBtn.addEventListener('click', async () => {
        const btnText = syncNtpBtn.querySelector('.btn-text');
        const btnLoader = syncNtpBtn.querySelector('.btn-loader');
        
        syncNtpBtn.disabled = true;
        btnText.style.display = 'none';
        btnLoader.style.display = 'inline-block';
        
        try {
            const response = await secureFetch('/api/ntp');
            if (response && response.ok) {
                const ntp = await response.json();
                
                updateElement('currentServer1', ntp.ntpServer1 || 'Belirtilmemiş');
                updateElement('currentServer2', ntp.ntpServer2 || 'Belirtilmemiş');
                updateElement('lastUpdate', new Date().toLocaleTimeString());
                
                loadCurrentNTPToInputs(ntp.ntpServer1, ntp.ntpServer2);
                
                showMessage('✅ NTP ayarları dsPIC\'ten başarıyla alındı', 'success');
            }
        } catch (error) {
            showMessage('❌ NTP ayarları alınamadı', 'error');
        } finally {
            syncNtpBtn.disabled = false;
            btnText.style.display = 'inline';
            btnLoader.style.display = 'none';
        }
    });
}

// İyileştirilmiş initNtpPage fonksiyonu
function initNtpPage() {
    const form = document.getElementById('ntpForm');
    if (!form) {
        console.warn('NTP form bulunamadı');
        return;
    }
    
    console.log('NTP sayfası başlatılıyor...');
    
    // Klavye handler'larını kur
    setupIPInputKeyboardHandlers();
    
    // Preset butonları ekle
    setTimeout(() => addPresetServerButtons(), 100);
    
    // Mevcut ayarları yükle
    secureFetch('/api/ntp')
    .then(r => r && r.json())
    .then(ntp => {
        if (ntp) {
            console.log('Mevcut NTP ayarları:', ntp);
            
            updateElement('currentServer1', ntp.ntpServer1 || 'Belirtilmemiş');
            updateElement('currentServer2', ntp.ntpServer2 || 'Belirtilmemiş');
            updateElement('lastUpdate', new Date().toLocaleTimeString());
            
            // Senkronizasyon durumu kontrolü
            if (ntp.syncStatus !== undefined) {
                const syncStatusEl = document.getElementById('syncStatus');
                if (syncStatusEl) {
                    if (ntp.syncStatus === 'synced') {
                        syncStatusEl.innerHTML = '<span style="color: var(--success);">✅ dsPIC ile senkron</span>';
                    } else {
                        syncStatusEl.innerHTML = '<span style="color: var(--warning);">⚠️ Lokal değerler</span>';
                    }
                }
            }
            
            // IP inputlarına yükle
            setTimeout(() => {
                loadCurrentNTPToInputs(
                    ntp.ntpServer1, 
                    ntp.ntpServer2,
                    ntp.subnet || '255.255.255.0',
                    ntp.gateway || '192.168.1.1',
                    ntp.dns || '8.8.8.8'
                );
            }, 200);
        }
    })
        .catch(error => {
            console.error('NTP ayarları yüklenemedi:', error);
            showMessage('NTP ayarları yüklenirken hata oluştu', 'error');
        });

    // Form gönderim handler'ı
    form.addEventListener('submit', async (e) => {
        e.preventDefault();
        
        console.log('NTP formu gönderiliyor...');
        
        // Validation
        if (!validateNTPForm()) {
            return;
        }
        
        const saveBtn = document.getElementById('saveNtpBtn');
        const btnText = saveBtn.querySelector('.btn-text');
        const btnLoader = saveBtn.querySelector('.btn-loader');
        
        // Loading state
        saveBtn.disabled = true;
        btnText.style.display = 'none';
        btnLoader.style.display = 'inline-block';
        
        const formData = new FormData(form);
        const server1 = formData.get('ntpServer1');
        const server2 = formData.get('ntpServer2');
        const subnet = formData.get('ntpSubnet');
        const gateway = formData.get('ntpGateway');
        const dns = formData.get('ntpDNS');
        
        console.log('Gönderilecek NTP ayarları:', { server1, server2, subnet, gateway, dns });
        
        try {
            const response = await secureFetch('/api/ntp', {
                method: 'POST',
                body: new URLSearchParams(formData)
            });
            
            if (response && response.ok) {
                showMessage('✅ NTP ayarları başarıyla dsPIC33EP ve Slave WT32\'ye gönderildi', 'success');
                
                // Mevcut değerleri göster
                updateElement('currentServer1', server1);
                updateElement('currentServer2', server2 || 'Belirtilmemiş');
                updateElement('lastUpdate', new Date().toLocaleTimeString());
                
            } else {
                const errorText = await response.text();
                showMessage('❌ NTP ayarları gönderilemedi: ' + errorText, 'error');
            }
        } catch (error) {
            console.error('NTP API hatası:', error);
            showMessage('⚠️ Sunucu ile iletişim kurulamadı', 'error');
        } finally {
            // Reset loading state
            saveBtn.disabled = false;
            btnText.style.display = 'inline';
            btnLoader.style.display = 'none';
        }
    });
    
    // Sayfa yüklendiğinde hidden input'ları başlat
    setTimeout(() => {
        updateHiddenIPInput(false);
        updateHiddenIPInput(true);
        updateHiddenIPInputForField('subnet');
        updateHiddenIPInputForField('gateway');
        updateHiddenIPInputForField('dns');
    }, 300);
    
    console.log('✅ NTP sayfası hazır');
}

// Yeni helper fonksiyon
function updateHiddenIPInputForField(fieldName) {
    const hiddenInput = document.getElementById('ntp' + fieldName.charAt(0).toUpperCase() + fieldName.slice(1));
    if (!hiddenInput) return;
    
    let parts = [];
    for (let i = 1; i <= 4; i++) {
        const input = document.getElementById(`${fieldName}-part${i}`);
        const value = input ? (input.value || '0') : '0';
        parts.push(value);
    }
    
    hiddenInput.value = parts.join('.');
}

function validateNTPForm() {
    const ntp1 = document.getElementById('ntpServer1').value;
    const subnet = document.getElementById('ntpSubnet').value;
    const gateway = document.getElementById('ntpGateway').value;
    const dns = document.getElementById('ntpDNS').value;
    
    console.log('NTP Form Validation:', { ntp1, subnet, gateway, dns });
    
    // Zorunlu alanlar
    if (!validateIPFormat(ntp1)) {
        showMessage('Lütfen geçerli bir birincil NTP IP adresi girin', 'error');
        return false;
    }
    
    if (!validateIPFormat(subnet)) {
        showMessage('Lütfen geçerli bir Subnet Mask girin', 'error');
        return false;
    }
    
    if (!validateIPFormat(gateway)) {
        showMessage('Lütfen geçerli bir Gateway IP adresi girin', 'error');
        return false;
    }
    
    if (!validateIPFormat(dns)) {
        showMessage('Lütfen geçerli bir DNS IP adresi girin', 'error');
        return false;
    }
    
    return true;
}

function loadCurrentNTPToInputs(server1, server2, subnet, gateway, dns) {
    console.log('NTP değerleri yükleniyor:', { server1, server2, subnet, gateway, dns });
    
    // NTP1 yükle
    if (server1 && validateIPFormat(server1)) {
        loadIPToContainer(server1, '.ip-input-container:not(:has(#ntp2-part1))');
        updateHiddenIPInput(false);
    }
    
    // NTP2 yükle
    if (server2 && validateIPFormat(server2)) {
        const parts = server2.split('.');
        for (let i = 1; i <= 4; i++) {
            const input = document.getElementById(`ntp2-part${i}`);
            if (input && parts[i-1]) {
                input.value = parts[i-1];
            }
        }
        updateHiddenIPInput(true);
    }
    
    // Subnet yükle
    if (subnet && validateIPFormat(subnet)) {
        const parts = subnet.split('.');
        for (let i = 1; i <= 4; i++) {
            const input = document.getElementById(`subnet-part${i}`);
            if (input && parts[i-1]) {
                input.value = parts[i-1];
            }
        }
        updateHiddenIPInputForField('subnet');
    }
    
    // Gateway yükle
    if (gateway && validateIPFormat(gateway)) {
        const parts = gateway.split('.');
        for (let i = 1; i <= 4; i++) {
            const input = document.getElementById(`gateway-part${i}`);
            if (input && parts[i-1]) {
                input.value = parts[i-1];
            }
        }
        updateHiddenIPInputForField('gateway');
    }
    
    // DNS yükle
    if (dns && validateIPFormat(dns)) {
        const parts = dns.split('.');
        for (let i = 1; i <= 4; i++) {
            const input = document.getElementById(`dns-part${i}`);
            if (input && parts[i-1]) {
                input.value = parts[i-1];
            }
        }
        updateHiddenIPInputForField('dns');
    }
}
    
// BaudRate Ayarları - GÜNCELLENMİŞ VERSİYON (Test butonu kaldırıldı)
function initBaudRatePage() {
    console.log("⚙️ BaudRate sayfası başlatılıyor...");
    
    const form = document.getElementById('baudrateForm');
    const checkBaudBtn = document.getElementById('checkBaudBtn');
    const saveBaudBtn = document.getElementById('saveBaudBtn');
    
    if (!form) {
        console.error('BaudRate form bulunamadı!');
        return;
    }

    // Son güncelleme zamanını güncelle
    function updateLastUpdateTime() {
        const now = new Date();
        const timeStr = now.toLocaleTimeString('tr-TR');
        updateElement('lastUpdate', timeStr);
    }

    // Mevcut baudrate'i sorgula
    async function checkCurrentBaudRate() {
        if (!checkBaudBtn) return;
        
        const btnText = checkBaudBtn.querySelector('.btn-text');
        const btnLoader = checkBaudBtn.querySelector('.btn-loader');
        
        try {
            // Loading state
            checkBaudBtn.disabled = true;
            if (btnText) btnText.style.display = 'none';
            if (btnLoader) btnLoader.style.display = 'inline-block';
            
            showMessage('Mevcut baudrate sorgulanıyor...', 'info');
            
            const response = await secureFetch('/api/baudrate/current');
            
            if (response && response.ok) {
                const data = await response.json();
                
                if (data.success && data.currentBaudRate > 0) {
                    updateElement('currentBaudRate', data.currentBaudRate + ' bps');
                    
                    // Radio butonunu seç
                    const radio = document.querySelector(`input[name="baud"][value="${data.currentBaudRate}"]`);
                    if (radio) {
                        radio.checked = true;
                    }
                    
                    showMessage(`Mevcut baudrate: ${data.currentBaudRate} bps`, 'success');
                } else {
                    updateElement('currentBaudRate', 'Alınamadı');
                    showMessage(data.message || 'Baudrate bilgisi alınamadı', 'error');
                }
            } else {
                updateElement('currentBaudRate', 'Hata');
                showMessage('Sunucu hatası', 'error');
            }
            
            updateLastUpdateTime();
            
        } catch (error) {
            console.error('Baudrate sorgulama hatası:', error);
            showMessage('Baudrate sorgulama hatası: ' + error.message, 'error');
            updateElement('currentBaudRate', 'Hata');
        } finally {
            // Reset loading state
            if (checkBaudBtn) checkBaudBtn.disabled = false;
            if (btnText) btnText.style.display = 'inline';
            if (btnLoader) btnLoader.style.display = 'none';
        }
    }

// BaudRate değiştir
    async function changeBaudRate(event) {
        event.preventDefault();
        
        const selectedBaud = document.querySelector('input[name="baud"]:checked');
        if (!selectedBaud) {
            showMessage('Lütfen bir baudrate seçin', 'error');
            return;
        }
        
        const btnText = saveBaudBtn?.querySelector('.btn-text');
        const btnLoader = saveBaudBtn?.querySelector('.btn-loader');
        
        try {
            // Loading state
            if (saveBaudBtn) saveBaudBtn.disabled = true;
            if (btnText) btnText.style.display = 'none';
            if (btnLoader) btnLoader.style.display = 'inline-block';
            
            showMessage(`BaudRate ${selectedBaud.value} bps olarak değiştiriliyor...`, 'info');
            
            const formData = new URLSearchParams();
            formData.append('baud', selectedBaud.value);
            
            const response = await secureFetch('/api/baudrate', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded'
                },
                body: formData
            });
            
            if (response && response.ok) {
                const data = await response.json();
                
                if (data.success) {
                    showMessage(`✅ BaudRate başarıyla ${data.newBaudRate} bps olarak değiştirildi`, 'success');
                    updateElement('currentBaudRate', data.newBaudRate + ' bps');
                    
                    // 2 saniye sonra otomatik kontrol
                    setTimeout(() => {
                        showMessage('Yeni ayar kontrol ediliyor...', 'info');
                        checkCurrentBaudRate();
                    }, 2000);
                } else {
                    showMessage(data.error || 'BaudRate değiştirilemedi', 'error');
                }
            } else {
                const errorText = response ? await response.text() : 'Bağlantı hatası';
                showMessage('BaudRate değiştirilemedi: ' + errorText, 'error');
            }
            
        } catch (error) {
            console.error('BaudRate değiştirme hatası:', error);
            showMessage('BaudRate değiştirme hatası: ' + error.message, 'error');
        } finally {
            // Reset loading state
            if (saveBaudBtn) saveBaudBtn.disabled = false;
            if (btnText) btnText.style.display = 'inline';
            if (btnLoader) btnLoader.style.display = 'none';
        }
    }

    // Event listener'ları ekle
    if (checkBaudBtn) {
        checkBaudBtn.addEventListener('click', checkCurrentBaudRate);
    }
    
    if (form) {
        form.addEventListener('submit', changeBaudRate);
    }

    // Sayfa yüklendiğinde son güncelleme zamanını göster
    updateLastUpdateTime();
    
    // İsteğe bağlı: Sayfa yüklendiğinde otomatik sorgulama yapma
    checkCurrentBaudRate();
    
    console.log('✅ BaudRate sayfası hazır');
}

// Global değişkenler (initFaultPage fonksiyonunun başına ekleyin)
let isPaused = false;
let isStopRequested = false;
let currentFaultIndex = 0;
let totalFaultCount = 0;

// Arıza sayfası başlatma fonksiyonuna eklenecek butonlar
function addPauseResumeButtons() {
    const controlPanel = document.querySelector('.control-buttons');
    if (!controlPanel) return;
    
    // Durdur/Devam Et butonu ekle
    const pauseResumeBtn = document.createElement('button');
    pauseResumeBtn.id = 'pauseResumeBtn';
    pauseResumeBtn.className = 'btn warning';
    pauseResumeBtn.style.display = 'none';
    pauseResumeBtn.innerHTML = `
        <span class="btn-icon">⏸️</span>
        <span class="btn-text">Duraklat</span>
    `;
    
    // İptal butonu ekle
    const stopBtn = document.createElement('button');
    stopBtn.id = 'stopBtn';
    stopBtn.className = 'btn danger';
    stopBtn.style.display = 'none';
    stopBtn.innerHTML = `
        <span class="btn-icon">⏹️</span>
        <span class="btn-text">İptal</span>
    `;
    
    controlPanel.appendChild(pauseResumeBtn);
    controlPanel.appendChild(stopBtn);
    
    // Event listeners
    pauseResumeBtn.addEventListener('click', togglePause);
    stopBtn.addEventListener('click', stopFetching);
}

// Duraklat/Devam Et
function togglePause() {
    isPaused = !isPaused;
    const btn = document.getElementById('pauseResumeBtn');
    const btnIcon = btn.querySelector('.btn-icon');
    const btnText = btn.querySelector('.btn-text');
    
    if (isPaused) {
        btnIcon.textContent = '▶️';
        btnText.textContent = 'Devam Et';
        btn.classList.remove('warning');
        btn.classList.add('success');
        updateElement('progressText', '⏸️ Duraklatıldı');
        showMessage('⏸️ Arıza çekme duraklatıldı', 'info');
    } else {
        btnIcon.textContent = '⏸️';
        btnText.textContent = 'Duraklat';
        btn.classList.remove('success');
        btn.classList.add('warning');
        updateElement('progressText', '▶️ Devam ediyor...');
        showMessage('▶️ Arıza çekme devam ediyor', 'info');
    }
}

// İptal et
function stopFetching() {
    if (confirm('Arıza çekme işlemi iptal edilecek. Emin misiniz?')) {
        isStopRequested = true;
        isPaused = false;
        showMessage('⏹️ İşlem iptal edildi', 'warning');
    }
}

// Arıza Kayıtları Sayfası - ULTRA HIZLI VERSİYON
function initFaultPage() {
    console.log("🛠️ Arıza Kayıtları sayfası başlatılıyor (Ultra Hızlı)...");
    
    const fetchAllFaultsBtn = document.getElementById('fetchAllFaultsBtn');
    const refreshFaultBtn = document.getElementById('refreshFaultBtn');
    const exportCSVBtn = document.getElementById('exportCSVBtn');
    const exportExcelBtn = document.getElementById('exportExcelBtn');
    const clearFaultBtn = document.getElementById('clearFaultBtn');
    const filterPinType = document.getElementById('filterPinType');
    const faultTableBody = document.getElementById('faultTableBody');
    const manualTestForm = document.getElementById('manualTestForm');
    const deleteFaultsFromDsPICBtn = document.getElementById('deleteFaultsFromDsPICBtn');
    
    if (!fetchAllFaultsBtn || !faultTableBody) {
        console.error("Fault page elementleri bulunamadı!");
        return;
    }
    
    let faultRecords = [];
    let filteredRecords = [];
    let isLoading = false;
    
    // PERFORMANS: Batch render için buffer
    let renderBuffer = [];
    let renderTimer = null;
    
    // Ham arıza verisini parse et (değişiklik yok)
    function parseFaultData(rawData) {
        console.log("Parse ediliyor:", rawData);
        
        const data = rawData.trim();
        
        let recordNumber = 0;
        let faultData = data;
        
        if (data.includes(':')) {
            const parts = data.split(':');
            recordNumber = parseInt(parts[0]);
            faultData = parts[1];
        }
        
        if (faultData.length < 22) {
            console.error("Çok kısa veri:", faultData);
            return null;
        }
        
        try {
            const pinHex = faultData.substring(0, 2);
            const pinNumber = parseInt(pinHex, 16);

            console.log(`Pin hex: ${pinHex} → decimal: ${pinNumber}`);
            
            let pinType, pinName, displayPinNumber;
            
            if (pinNumber >= 1 && pinNumber <= 8) {
                pinType = "Çıkış";
                pinName = "Çıkış " + pinNumber;
                displayPinNumber = pinNumber;
            } else if (pinNumber >= 9 && pinNumber <= 16) {
                pinType = "Giriş";
                const adjustedPinNumber = pinNumber - 8;
                pinName = "Giriş " + adjustedPinNumber;
                displayPinNumber = adjustedPinNumber;
            } else {
                pinType = "Bilinmeyen";
                pinName = "Pin " + pinNumber;
                displayPinNumber = pinNumber;
            }
            
            const year = 2000 + parseInt(faultData.substring(2, 4), 10);
            const month = parseInt(faultData.substring(4, 6), 10);
            const day = parseInt(faultData.substring(6, 8), 10);
            const hour = parseInt(faultData.substring(8, 10), 10);
            const minute = parseInt(faultData.substring(10, 12), 10);
            const second = parseInt(faultData.substring(12, 14), 10);
            
            console.log("Tarih:", {year, month, day, hour, minute, second});
            
            if (month < 1 || month > 12 || day < 1 || day > 31 || 
                hour > 23 || minute > 59 || second > 59) {
                console.error("Geçersiz tarih-saat!");
                return null;
            }
            
            const dateTime = `${day.toString().padStart(2, '0')}/${month.toString().padStart(2, '0')}/${year} ` +
                            `${hour.toString().padStart(2, '0')}:${minute.toString().padStart(2, '0')}:${second.toString().padStart(2, '0')}`;
            
            let millisecond = 0;
            if (faultData.length >= 17) {
                millisecond = parseInt(faultData.substring(14, 17), 10);
                console.log(`Milisaniye: ${millisecond} ms`);
            }
            
            let duration = "0.000 sn";
            let durationSeconds = 0;
            
            if (faultData.length >= 22) {
                const durationStr = faultData.substring(17, 22);
                const seconds = parseInt(durationStr.substring(0, 2), 10);
                const ms = parseInt(durationStr.substring(2, 5), 10);
                durationSeconds = seconds + (ms / 1000.0);
                
                console.log(`Süre: ${seconds}.${ms} = ${durationSeconds} saniye`);
                
                if (durationSeconds < 1.0) {
                    duration = Math.round(durationSeconds * 1000) + " ms";
                } else if (durationSeconds < 60.0) {
                    duration = durationSeconds.toFixed(3) + " sn";
                } else {
                    const mins = Math.floor(durationSeconds / 60);
                    const secs = durationSeconds % 60;
                    duration = mins + "dk " + secs.toFixed(1) + "sn";
                }
            }
            
            return {
                recordNumber,
                pinNumber: displayPinNumber,
                actualPinNumber: pinNumber,
                pinType,
                pinName,
                dateTime,
                duration,
                durationSeconds,
                millisecond,
                rawData: data
            };
            
        } catch (error) {
            console.error("Parse hatası:", error);
            return null;
        }
    }
    
    // Arıza kaydı ekleme
    function addFaultToTable(fault, index, faultNo) {
        const row = document.createElement('tr');
        row.className = 'fault-row';
        
        const pinBadgeClass = fault.pinType === 'Çıkış' ? 'output' : 'input';
        const dateTimeWithMs = `${fault.dateTime}.${fault.millisecond}`;
        const displayOrder = faultNo;
        
        row.innerHTML = `
            <td class="text-center">${displayOrder}</td>
            <td class="text-center"><span class="fault-number-badge">${faultNo.toString().padStart(5, '0')}</span></td>
            <td class="text-center">${fault.pinNumber}</td>
            <td><span class="pin-badge ${pinBadgeClass}">${fault.pinType}</span></td>
            <td class="datetime-cell">${dateTimeWithMs}</td>
            <td class="duration-cell">${fault.duration}</td>
            <td class="raw-data-cell" title="${fault.rawData}">${fault.rawData}</td>
        `;
        
        return row;
    }
    
    // PERFORMANS: Batch rendering
    function batchRenderTable() {
        if (renderTimer) {
            clearTimeout(renderTimer);
        }
        
        renderTimer = setTimeout(() => {
            if (renderBuffer.length === 0) return;
            
            const fragment = document.createDocumentFragment();
            const recordsToRender = renderBuffer.splice(0, 100); // 100'lük gruplar halinde render et
            
            recordsToRender.forEach(record => {
                const row = addFaultToTable(record, 0, record.faultNo);
                fragment.appendChild(row);
            });
            
            faultTableBody.appendChild(fragment);
            updateElement('totalFaults', faultRecords.length.toString());
            
            // Hala render edilecek kayıt varsa devam et
            if (renderBuffer.length > 0) {
                batchRenderTable();
            }
        }, 50); // 50ms gecikme ile render
    }
    
    // Tablo güncelleme
    function updateTable() {
        const filterType = filterPinType ? filterPinType.value : 'all';
        filteredRecords = filterType === 'all' ? 
            [...faultRecords] : 
            faultRecords.filter(record => record.pinType === filterType);
        
        faultTableBody.innerHTML = '';
        
        if (filteredRecords.length === 0) {
            faultTableBody.innerHTML = `
                <tr class="empty-row">
                    <td colspan="7" class="empty-state">
                        <div class="empty-icon">🔍</div>
                        <h4>Arıza kaydı bulunamadı</h4>
                    </td>
                </tr>
            `;
            return;
        }
        
        // Batch render kullan
        renderBuffer = [...filteredRecords];
        batchRenderTable();
    }
    
    // Progress bar güncelleme
    function updateProgress(current, total) {
        const percent = Math.round((current / total) * 100);
        
        updateElement('progressCurrent', current.toString());
        updateElement('progressTotal', total.toString());
        updateElement('progressPercent', percent + '%');
        
        const progressBar = document.getElementById('progressBar');
        if (progressBar) {
            progressBar.style.width = percent + '%';
        }
    }
    
    // AN komutuyla arıza sayısını al
    async function getFaultCount() {
        try {
            console.log("📊 Arıza sayısı sorgulanıyor (AN komutu)...");
            
            const formData = new URLSearchParams();
            formData.append('command', 'AN');
            
            const response = await secureFetch('/api/uart/send', {
                method: 'POST',
                body: formData
            });
            
            if (response && response.ok) {
                const data = await response.json();
                
                if (data.success && data.response) {
                    const responseText = data.response.trim();
                    console.log(`📥 Gelen yanıt: ${responseText}`);
                    
                    if (responseText.startsWith('A') && responseText.length >= 5) {
                        const numberStr = responseText.substring(1);
                        const count = parseInt(numberStr, 10);
                        const actualFaultCount = count - 1;
                        
                        console.log(`✅ Sistem arıza sayısı: ${actualFaultCount}`);
                        updateElement('systemFaultCount', actualFaultCount.toString());
                        return actualFaultCount;
                    }
                }
            }
            
            console.error("❌ Arıza sayısı alınamadı");
            return 0;
            
        } catch (error) {
            console.error("Arıza sayısı sorgu hatası:", error);
            return 0;
        }
    }

    // YAVAŞ AMA GARANTİLİ ARIZA ÇEKME
async function fetchAllFaultsReliable() {
    if (isLoading) return;
    isLoading = true;
    isPaused = false;
    isStopRequested = false;
    
    const fetchAllFaultsBtn = document.getElementById('fetchAllFaultsBtn');
    const pauseResumeBtn = document.getElementById('pauseResumeBtn');
    const stopBtn = document.getElementById('stopBtn');
    const btnText = fetchAllFaultsBtn.querySelector('.btn-text');
    const btnIcon = fetchAllFaultsBtn.querySelector('.btn-icon');
    const btnLoader = fetchAllFaultsBtn.querySelector('.btn-loader');
    const progressSection = document.getElementById('progressSection');
    
    // UI'ı loading durumuna al
    fetchAllFaultsBtn.disabled = true;
    btnIcon.style.display = 'none';
    btnLoader.style.display = 'inline-block';
    btnText.textContent = 'Çekiliyor...';
    
    // Kontrol butonlarını göster
    if (pauseResumeBtn) pauseResumeBtn.style.display = 'inline-flex';
    if (stopBtn) stopBtn.style.display = 'inline-flex';
    
    // Önceki verileri temizle
    faultRecords = [];
    renderBuffer = [];
    faultTableBody.innerHTML = '';
    
    const startTime = Date.now();
    
    try {
        // 1. Arıza sayısını al
        progressSection.style.display = 'block';
        updateElement('progressText', 'Arıza sayısı sorgulanıyor...');
        
        totalFaultCount = await getFaultCount();
        
        if (totalFaultCount === 0) {
            showMessage('❌ Sistemde arıza kaydı bulunamadı', 'warning');
            progressSection.style.display = 'none';
            return;
        }
        
        showMessage(`✅ ${totalFaultCount} arıza bulundu, güvenli modda çekiliyor...`, 'info');
        updateProgress(0, totalFaultCount);
        
        let successCount = 0;
        let failCount = 0;
        let consecutiveFailures = 0;
        
        // TEK TEK VE YAVAŞ ÇEK (En yeniden en eskiye)
        for (let faultNo = totalFaultCount; faultNo >= 1; faultNo--) {
            // İptal kontrolü
            if (isStopRequested) {
                showMessage('⏹️ Kullanıcı tarafından iptal edildi', 'warning');
                break;
            }
            
            // Duraklatma kontrolü
            while (isPaused && !isStopRequested) {
                await new Promise(resolve => setTimeout(resolve, 100));
            }
            
            currentFaultIndex = faultNo;
            
            // Progress güncelle
            updateProgress(totalFaultCount - faultNo + 1, totalFaultCount);
            updateElement('progressText', 
                `Arıza ${faultNo}/${totalFaultCount} çekiliyor...`);
            
            // Arızayı çek (maksimum 5 deneme)
            let fault = null;
            let attempts = 0;
            const maxAttempts = 5;
            
            while (attempts < maxAttempts && !fault) {
                attempts++;
                
                if (attempts > 1) {
                    console.log(`🔄 Deneme ${attempts}/${maxAttempts} - Arıza ${faultNo}`);
                    updateElement('progressText', 
                        `Arıza ${faultNo} - Deneme ${attempts}/${maxAttempts}`);
                    
                    // Her denemede artan bekleme süresi
                    await new Promise(resolve => setTimeout(resolve, 500 * attempts));
                }
                
                // Arızayı çek
                fault = await getSingleFaultSafe(faultNo);
                
                if (!fault && attempts < maxAttempts) {
                    // Başarısızsa buffer'ı temizle ve bekle
                    clearUARTBufferJS();
                    await new Promise(resolve => setTimeout(resolve, 300));
                }
            }
            
            if (fault) {
                successCount++;
                consecutiveFailures = 0;
                fault.displayOrder = faultNo;
                fault.faultNo = faultNo;
                faultRecords.push(fault);
                
                // Hemen tabloya ekle (her kayıt anında görünsün)
                addSingleFaultToTable(fault, faultNo);
                
                console.log(`✅ Arıza ${faultNo} başarıyla alındı`);
                
                // Her başarılı çekimden sonra kısa bekleme
                await new Promise(resolve => setTimeout(resolve, 300));
                
            } else {
                failCount++;
                consecutiveFailures++;
                console.error(`❌ Arıza ${faultNo} alınamadı (${maxAttempts} deneme başarısız)`);
                
                // Çok fazla ardışık hata varsa biraz daha bekle
                if (consecutiveFailures > 3) {
                    console.log('⚠️ Ardışık hatalar, 2 saniye bekleniyor...');
                    await new Promise(resolve => setTimeout(resolve, 2000));
                }
            }
            
            // Her 10 kayıtta bir durum raporu
            if ((totalFaultCount - faultNo + 1) % 10 === 0) {
                console.log(`📊 Durum: ${successCount} başarılı, ${failCount} başarısız`);
            }
        }
        
        // İşlem tamamlandı
        updateProgress(totalFaultCount, totalFaultCount);
        updateElement('progressText', isStopRequested ? '⏹️ İptal edildi' : '✅ Tamamlandı!');
        updateElement('lastQuery', new Date().toLocaleTimeString());
        
        const elapsedTime = Math.round((Date.now() - startTime) / 1000);
        
        showMessage(
            `${isStopRequested ? '⏹️' : '✅'} ${successCount}/${totalFaultCount} kayıt ${elapsedTime} saniyede alındı` +
            (failCount > 0 ? ` - ${failCount} başarısız` : ''), 
            isStopRequested ? 'warning' : 'success'
        );
        
        setTimeout(() => {
            progressSection.style.display = 'none';
        }, 2000);
        
    } catch (error) {
        console.error('Arıza çekme hatası:', error);
        showMessage('❌ ' + error.message, 'error');
        progressSection.style.display = 'none';
        
    } finally {
        isLoading = false;
        isStopRequested = false;
        isPaused = false;
        currentFaultIndex = 0;
        
        // Butonları resetle
        fetchAllFaultsBtn.disabled = false;
        btnIcon.style.display = 'inline';
        btnLoader.style.display = 'none';
        btnIcon.textContent = '📥';
        btnText.textContent = 'Tüm Arızaları İste';
        
        // Kontrol butonlarını gizle
        if (pauseResumeBtn) pauseResumeBtn.style.display = 'none';
        if (stopBtn) stopBtn.style.display = 'none';
    }
}

// GÜVENLİ TEK ARIZA ÇEKME
async function getSingleFaultSafe(faultNo) {
    try {
        const command = faultNo.toString().padStart(5, '0') + 'v';
        const formData = new URLSearchParams();
        formData.append('command', command);
        
        // Daha uzun timeout
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 2000); // 2 saniye
        
        const response = await fetch('/api/uart/send', {
            method: 'POST',
            body: formData,
            signal: controller.signal,
            headers: {
                'Authorization': `Bearer ${state.token}`
            }
        });
        
        clearTimeout(timeoutId);
        
        if (response && response.ok) {
            const data = await response.json();
            
            if (data.success && data.response && data.response.length > 10) {
                // Response'u kontrol et
                if (data.response === 'E' || data.response.includes('ERROR')) {
                    return null;
                }
                
                const parsedFault = parseFaultData(data.response);
                if (parsedFault) {
                    parsedFault.faultNo = faultNo;
                    return parsedFault;
                }
            }
        }
        return null;
        
    } catch (error) {
        if (error.name === 'AbortError') {
            console.warn(`⏱️ Timeout: Arıza ${faultNo}`);
        }
        return null;
    }
}

// Tabloya tek arıza ekle (anında göster)
function addSingleFaultToTable(fault, faultNo) {
    const faultTableBody = document.getElementById('faultTableBody');
    if (!faultTableBody) return;
    
    // Boş satır varsa kaldır
    const emptyRow = faultTableBody.querySelector('.empty-row');
    if (emptyRow) {
        emptyRow.remove();
    }
    
    const row = document.createElement('tr');
    row.className = 'fault-row new-row';
    
    const pinBadgeClass = fault.pinType === 'Çıkış' ? 'output' : 'input';
    const dateTimeWithMs = `${fault.dateTime}.${fault.millisecond || '000'}`;
    
    row.innerHTML = `
        <td class="text-center">${faultTableBody.children.length + 1}</td>
        <td class="text-center"><span class="fault-number-badge">${faultNo.toString().padStart(5, '0')}</span></td>
        <td class="text-center">${fault.pinNumber}</td>
        <td><span class="pin-badge ${pinBadgeClass}">${fault.pinType}</span></td>
        <td class="datetime-cell">${dateTimeWithMs}</td>
        <td class="duration-cell">${fault.duration}</td>
        <td class="raw-data-cell" title="${fault.rawData}">${fault.rawData}</td>
    `;
    
    // Tablonun başına ekle (en yeni en üstte)
    faultTableBody.insertBefore(row, faultTableBody.firstChild);
    
    // Satır sayısını güncelle
    updateElement('totalFaults', (faultTableBody.children.length).toString());
    
    // Animasyon için
    setTimeout(() => {
        row.classList.remove('new-row');
    }, 300);
}

// JavaScript tarafında buffer temizleme
function clearUARTBufferJS() {
    // Bu fonksiyon sadece bekleme amaçlı
    console.log('🔧 Buffer temizleniyor...');
}
    // dsPIC'TEKİ ARIZALARI SİL
    async function deleteFaultsFromDsPIC() {
        const firstConfirm = confirm(
            '⚠️ DİKKAT!\n\n' +
            'dsPIC33EP üzerindeki TÜM arıza kayıtları silinecek.\n' +
            'Bu işlem GERİ ALINAMAZ!\n\n' +
            'Devam etmek istiyor musunuz?'
        );
        
        if (!firstConfirm) return;
        
        const secondConfirm = confirm(
            '❌ SON UYARI!\n\n' +
            'Bu işlem dsPIC33EP hafızasındaki tüm arıza kayıtlarını kalıcı olarak silecektir.\n' +
            'Emin misiniz?'
        );
        
        if (!secondConfirm) return;
        
        const btnText = deleteFaultsFromDsPICBtn.querySelector('.btn-text');
        const btnIcon = deleteFaultsFromDsPICBtn.querySelector('.btn-icon');
        const btnLoader = deleteFaultsFromDsPICBtn.querySelector('.btn-loader');
        
        deleteFaultsFromDsPICBtn.disabled = true;
        btnIcon.style.display = 'none';
        btnLoader.style.display = 'inline-block';
        btnText.textContent = 'Siliniyor...';
        
        try {
            console.log("🗑️ dsPIC arızaları siliniyor (tT komutu)...");
            
            const formData = new URLSearchParams();
            formData.append('command', 'tT');
            
            const response = await secureFetch('/api/uart/send', {
                method: 'POST',
                body: formData
            });
            
            if (response && response.ok) {
                const data = await response.json();
                
                if (data.success) {
                    showMessage('✅ dsPIC33EP üzerindeki tüm arıza kayıtları başarıyla silindi!', 'success');
                    
                    faultRecords = [];
                    renderBuffer = [];
                    updateTable();
                    updateElement('systemFaultCount', '0');
                    
                    console.log('✅ tT komutu gönderildi, yanıt:', data.response);
                } else {
                    showMessage('❌ Silme işlemi başarısız oldu', 'error');
                    console.error('tT komutu başarısız:', data);
                }
            } else {
                showMessage('❌ Sunucu hatası', 'error');
            }
            
        } catch (error) {
            console.error('dsPIC arıza silme hatası:', error);
            showMessage('❌ Silme işlemi sırasında hata oluştu', 'error');
            
        } finally {
            deleteFaultsFromDsPICBtn.disabled = false;
            btnIcon.style.display = 'inline';
            btnLoader.style.display = 'none';
            btnIcon.textContent = '⚠️';
            btnText.textContent = 'dsPIC Arızalarını Sil';
        }
    }
    
    // Event listener'lar
    
    // Ana buton - Ultra hızlı versiyona bağla
    fetchAllFaultsBtn.addEventListener('click', fetchAllFaultsReliable);

    // Sayfa yüklendiğinde butonları ekle
    addPauseResumeButtons();
    
    // dsPIC arızalarını sil butonu
    if (deleteFaultsFromDsPICBtn) {
        deleteFaultsFromDsPICBtn.addEventListener('click', deleteFaultsFromDsPIC);
    }
    
    // Yenile butonu
    if (refreshFaultBtn) {
        refreshFaultBtn.addEventListener('click', () => {
            updateTable();
            showMessage('✅ Tablo yenilendi', 'info');
        });
    }
    
    // Temizle butonu
    if (clearFaultBtn) {
        clearFaultBtn.addEventListener('click', () => {
            if (faultRecords.length === 0) {
                showMessage('Temizlenecek kayıt yok', 'warning');
                return;
            }
            
            if (confirm(`${faultRecords.length} adet arıza kaydını tablodan temizlemek istediğinizden emin misiniz?`)) {
                faultRecords = [];
                renderBuffer = [];
                updateTable();
                updateElement('systemFaultCount', '-');
                showMessage('✅ Tablo temizlendi', 'success');
            }
        });
    }
    
    // Filtre değişimi
    if (filterPinType) {
        filterPinType.addEventListener('change', updateTable);
    }
    
    // CSV Export
    if (exportCSVBtn) {
        exportCSVBtn.addEventListener('click', () => {
            if (faultRecords.length === 0) {
                showMessage('❌ Dışa aktarılacak arıza kaydı bulunamadı', 'warning');
                return;
            }
            
            exportFaultsAsCSV(filteredRecords.length > 0 ? filteredRecords : faultRecords);
        });
    }
    
    // Excel Export
    if (exportExcelBtn) {
        exportExcelBtn.addEventListener('click', () => {
            if (faultRecords.length === 0) {
                showMessage('❌ Dışa aktarılacak arıza kaydı bulunamadı', 'warning');
                return;
            }
            
            exportFaultsAsExcel(filteredRecords.length > 0 ? filteredRecords : faultRecords);
        });
    }
    
    // Manuel test form handler (değişiklik yok - aynı kalacak)
    if (manualTestForm) {
        document.querySelectorAll('.quick-commands .btn').forEach(btn => {
            btn.addEventListener('click', function() {
                const command = this.dataset.cmd;
                const commandInput = document.getElementById('manualCommand');
                if (commandInput) {
                    commandInput.value = command;
                    commandInput.focus();
                }
            });
        });
        
        manualTestForm.addEventListener('submit', async function(e) {
            e.preventDefault();
            
            const command = document.getElementById('manualCommand').value.trim();
            if (!command) {
                showMessage('Komut boş olamaz', 'warning');
                return;
            }
            
            const submitBtn = this.querySelector('button[type="submit"]');
            const btnText = submitBtn.querySelector('.btn-text');
            const btnLoader = submitBtn.querySelector('.btn-loader');
            
            submitBtn.disabled = true;
            btnText.style.display = 'none';
            btnLoader.style.display = 'inline-block';
            
            try {
                const formData = new URLSearchParams();
                formData.append('command', command);
                
                const response = await secureFetch('/api/uart/send', {
                    method: 'POST',
                    body: formData
                });
                
                if (response && response.ok) {
                    const data = await response.json();
                    showManualTestResult(data);
                } else {
                    showMessage('❌ Manuel test başarısız oldu', 'error');
                }
                
            } catch (error) {
                console.error('Manuel test hatası:', error);
                showMessage('❌ Manuel test sırasında hata oluştu', 'error');
            } finally {
                submitBtn.disabled = false;
                btnText.style.display = 'inline';
                btnLoader.style.display = 'none';
            }
        });
        
        document.getElementById('clearManualTest')?.addEventListener('click', function() {
            document.getElementById('manualCommand').value = '';
            document.getElementById('manualTestResult').style.display = 'none';
            showMessage('Manuel test alanı temizlendi', 'info');
        });
    }
    
    // Manuel test sonucunu göster
    function showManualTestResult(data) {
        const resultDiv = document.getElementById('manualTestResult');
        const contentDiv = document.getElementById('manualTestContent');
        
        if (!resultDiv || !contentDiv) return;
        
        let resultHTML = `
            <div class="test-result-item">
                <strong>Gönderilen Komut:</strong>
                <code style="font-family: monospace; background: var(--bg-tertiary); padding: 2px 6px; border-radius: 3px;">${data.command}</code>
            </div>
            <div class="test-result-item">
                <strong>Durum:</strong>
                <span class="status-badge ${data.success ? 'active' : 'error'}">
                    ${data.success ? 'Başarılı' : 'Başarısız'}
                </span>
            </div>
            <div class="test-result-item">
                <strong>Yanıt Uzunluğu:</strong>
                <span>${data.responseLength} karakter</span>
            </div>
            <div class="test-result-item">
                <strong>Zaman:</strong>
                <span>${data.timestamp}</span>
            </div>
        `;
        
        if (data.responseLength > 0) {
            resultHTML += `
                <div class="test-result-item" style="flex-direction: column; align-items: flex-start;">
                    <strong style="margin-bottom: 0.5rem;">dsPIC Yanıtı:</strong>
                    <div class="test-result-response" style="
                        background: var(--bg-tertiary); 
                        padding: 0.5rem; 
                        border-radius: 4px; 
                        font-family: monospace; 
                        font-size: 0.875rem;
                        word-break: break-all;
                        width: 100%;">
                        ${escapeHtml(data.response)}
                    </div>
                </div>
            `;
        } else {
            resultHTML += `
                <div class="test-result-item">
                    <strong>dsPIC Yanıtı:</strong>
                    <span class="test-result-empty" style="color: var(--text-tertiary);">Yanıt alınamadı</span>
                </div>
            `;
        }
        
        contentDiv.innerHTML = resultHTML;
        resultDiv.style.display = 'block';
        resultDiv.scrollIntoView({ behavior: 'smooth' });
        
        showMessage(
            data.success ? 
            `✅ Komut başarılı: ${data.responseLength} karakter yanıt` : 
            '❌ Komut başarısız (timeout)', 
            data.success ? 'success' : 'error'
        );
    }
    
    // CSV Export fonksiyonu (değişiklik yok)
    function exportFaultsAsCSV(records) {
        try {
            const BOM = '\uFEFF';
            let csvContent = 'sep=;\n';
            
            csvContent += '"Sıra";"Arıza No";"Pin No";"Pin Tipi";"Pin Adı";"Tarih-Saat";"Arıza Süresi";"Süre (sn)";"Ham Veri"\n';
            
            records.forEach((record, index) => {
                const dateTimeWithMs = `${record.dateTime}.${record.millisecond}`;
                const faultNo = record.faultNo ? record.faultNo.toString().padStart(5, '0') : (index + 1).toString().padStart(5, '0');
                
                const row = [
                    index + 1,
                    faultNo,
                    record.pinNumber,
                    record.pinType,
                    record.pinName,
                    dateTimeWithMs,
                    record.duration,
                    record.durationSeconds || 0,
                    record.rawData
                ];
                
                const escapedRow = row.map(field => {
                    const str = String(field).replace(/"/g, '""');
                    return `"${str}"`;
                });
                
                csvContent += escapedRow.join(';') + '\n';
            });
            
            const blob = new Blob([BOM + csvContent], { 
                type: 'text/csv;charset=utf-8' 
            });
            
            const now = new Date();
            const dateStr = now.toISOString().slice(0, 10);
            const timeStr = now.toTimeString().slice(0, 5).replace(':', '');
            const filename = `teias_eklim_faults_${dateStr}_${timeStr}.csv`;
            
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = filename;
            a.style.display = 'none';
            
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
            
            showMessage(`✅ ${records.length} arıza kaydı CSV olarak dışa aktarıldı`, 'success');
            
        } catch (error) {
            console.error('CSV export hatası:', error);
            showMessage('❌ CSV dışa aktarma sırasında hata oluştu', 'error');
        }
    }

    // Excel Export fonksiyonu (değişiklik yok)
    function exportFaultsAsExcel(records) {
        try {
            let xmlContent = '<?xml version="1.0" encoding="UTF-8"?>\n';
            xmlContent += '<Workbook xmlns="urn:schemas-microsoft-com:office:spreadsheet"\n';
            xmlContent += ' xmlns:o="urn:schemas-microsoft-com:office:office"\n';
            xmlContent += ' xmlns:x="urn:schemas-microsoft-com:office:excel"\n';
            xmlContent += ' xmlns:ss="urn:schemas-microsoft-com:office:spreadsheet"\n';
            xmlContent += ' xmlns:html="https://www.w3.org/TR/REC-html40">\n';
            
            xmlContent += '<DocumentProperties xmlns="urn:schemas-microsoft-com:office:office">\n';
            xmlContent += '<Title>TEİAŞ EKLİM Arıza Kayıtları</Title>\n';
            xmlContent += '<Author>TEİAŞ EKLİM Sistemi</Author>\n';
            xmlContent += '<Created>' + new Date().toISOString() + '</Created>\n';
            xmlContent += '<Company>TEİAŞ</Company>\n';
            xmlContent += '</DocumentProperties>\n';
            
            xmlContent += '<Styles>\n';
            xmlContent += '<Style ss:ID="Header">\n';
            xmlContent += '<Font ss:FontName="Calibri" ss:Size="11" ss:Color="#FFFFFF" ss:Bold="1"/>\n';
            xmlContent += '<Interior ss:Color="#4F81BD" ss:Pattern="Solid"/>\n';
            xmlContent += '<Borders>\n';
            xmlContent += '<Border ss:Position="Bottom" ss:LineStyle="Continuous" ss:Weight="1"/>\n';
            xmlContent += '<Border ss:Position="Left" ss:LineStyle="Continuous" ss:Weight="1"/>\n';
            xmlContent += '<Border ss:Position="Right" ss:LineStyle="Continuous" ss:Weight="1"/>\n';
            xmlContent += '<Border ss:Position="Top" ss:LineStyle="Continuous" ss:Weight="1"/>\n';
            xmlContent += '</Borders>\n';
            xmlContent += '</Style>\n';
            
            xmlContent += '<Style ss:ID="Output">\n';
            xmlContent += '<Font ss:FontName="Calibri" ss:Size="11" ss:Color="#006100"/>\n';
            xmlContent += '<Interior ss:Color="#C6EFCE" ss:Pattern="Solid"/>\n';
            xmlContent += '</Style>\n';
            
            xmlContent += '<Style ss:ID="Input">\n';
            xmlContent += '<Font ss:FontName="Calibri" ss:Size="11" ss:Color="#0F1494"/>\n';
            xmlContent += '<Interior ss:Color="#B7DEE8" ss:Pattern="Solid"/>\n';
            xmlContent += '</Style>\n';
            
            xmlContent += '</Styles>\n';
            
            xmlContent += '<Worksheet ss:Name="Arıza Kayıtları">\n';
            xmlContent += '<Table ss:ExpandedColumnCount="8" ss:ExpandedRowCount="' + (records.length + 1) + '" x:FullColumns="1" x:FullRows="1">\n';
            
            xmlContent += '<Column ss:AutoFitWidth="0" ss:Width="50"/>\n';
            xmlContent += '<Column ss:AutoFitWidth="0" ss:Width="60"/>\n';
            xmlContent += '<Column ss:AutoFitWidth="0" ss:Width="70"/>\n';
            xmlContent += '<Column ss:AutoFitWidth="0" ss:Width="100"/>\n';
            xmlContent += '<Column ss:AutoFitWidth="0" ss:Width="160"/>\n';
            xmlContent += '<Column ss:AutoFitWidth="0" ss:Width="100"/>\n';
            xmlContent += '<Column ss:AutoFitWidth="0" ss:Width="80"/>\n';
            xmlContent += '<Column ss:AutoFitWidth="0" ss:Width="150"/>\n';
            
            xmlContent += '<Row ss:StyleID="Header">\n';
            xmlContent += '<Cell><Data ss:Type="String">Sıra</Data></Cell>\n';
            xmlContent += '<Cell><Data ss:Type="String">Pin No</Data></Cell>\n';
            xmlContent += '<Cell><Data ss:Type="String">Pin Tipi</Data></Cell>\n';
            xmlContent += '<Cell><Data ss:Type="String">Pin Adı</Data></Cell>\n';
            xmlContent += '<Cell><Data ss:Type="String">Tarih-Saat</Data></Cell>\n';
            xmlContent += '<Cell><Data ss:Type="String">Arıza Süresi</Data></Cell>\n';
            xmlContent += '<Cell><Data ss:Type="String">Süre (sn)</Data></Cell>\n';
            xmlContent += '<Cell><Data ss:Type="String">Ham Veri</Data></Cell>\n';
            xmlContent += '</Row>\n';
            
            records.forEach((record, index) => {
                const styleID = record.pinType === 'Çıkış' ? 'Output' : 'Input';
                const dateTimeWithMs = `${record.dateTime}.${record.millisecond}`;
                
                xmlContent += `<Row ss:StyleID="${styleID}">\n`;
                xmlContent += `<Cell><Data ss:Type="Number">${index + 1}</Data></Cell>\n`;
                xmlContent += `<Cell><Data ss:Type="Number">${record.pinNumber}</Data></Cell>\n`;
                xmlContent += `<Cell><Data ss:Type="String">${escapeXml(record.pinType)}</Data></Cell>\n`;
                xmlContent += `<Cell><Data ss:Type="String">${escapeXml(record.pinName)}</Data></Cell>\n`;
                xmlContent += `<Cell><Data ss:Type="String">${escapeXml(dateTimeWithMs)}</Data></Cell>\n`;
                xmlContent += `<Cell><Data ss:Type="String">${escapeXml(record.duration)}</Data></Cell>\n`;
                xmlContent += `<Cell><Data ss:Type="Number">${record.durationSeconds || 0}</Data></Cell>\n`;
                xmlContent += `<Cell><Data ss:Type="String">${escapeXml(record.rawData)}</Data></Cell>\n`;
                xmlContent += '</Row>\n';
            });
            
            xmlContent += '</Table>\n';
            xmlContent += '</Worksheet>\n';
            xmlContent += '</Workbook>';
            
            function escapeXml(str) {
                if (!str) return '';
                return str.toString().replace(/[<>&'"]/g, function (c) {
                    switch (c) {
                        case '<': return '&lt;';
                        case '>': return '&gt;';
                        case '&': return '&amp;';
                        case "'": return '&apos;';
                        case '"': return '&quot;';
                        default: return c;
                    }
                });
            }
            
            const BOM = '\uFEFF';
            const blob = new Blob([BOM + xmlContent], { 
                type: 'application/vnd.ms-excel;charset=utf-8' 
            });
            
            const now = new Date();
            const dateStr = now.toISOString().slice(0, 10);
            const timeStr = now.toTimeString().slice(0, 5).replace(':', '');
            const filename = `teias_eklim_faults_${dateStr}_${timeStr}.xls`;
            
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = filename;
            a.style.display = 'none';
            
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
            
            showMessage(`✅ ${records.length} arıza kaydı renkli Excel formatında dışa aktarıldı`, 'success');
            
        } catch (error) {
            console.error('Excel export hatası:', error);
            showMessage('❌ Excel dışa aktarma sırasında hata oluştu', 'error');
        }
    }

    // Helper functions
    function escapeHtml(text) {
        const map = {
            '&': '&amp;',
            '<': '&lt;',
            '>': '&gt;',
            '"': '&quot;',
            "'": '&#039;'
        };
        return text.replace(/[&<>"']/g, function(m) { return map[m]; });
    }
    
    function updateElement(id, value) {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = value;
        }
    }
    
    // İlk yüklemede tabloyu boş göster
    updateTable();
    
    console.log('✅ Ultra Hızlı Fault sayfası hazır');
}

// Log Kayıtları Sayfası - Otomatik Kaydırma KALDIRILMIŞ, Pagination DESTEKLİ
function initLogPage() {
    const logContainer = document.getElementById('logContainer');
    const pauseLogsBtn = document.getElementById('pauseLogsBtn');
    const exportLogsBtn = document.getElementById('exportLogsBtn');
    const exportExcelBtn = document.getElementById('exportExcelBtn');
    const refreshLogsBtn = document.getElementById('refreshLogsBtn');
    const clearLogsBtn = document.getElementById('clearLogsBtn');
    const autoRefreshToggle = document.getElementById('autoRefreshToggle');
    const refreshInterval = document.getElementById('refreshInterval');
    const logSearch = document.getElementById('logSearch');
    const logLevelFilter = document.getElementById('logLevelFilter');
    const logSourceFilter = document.getElementById('logSourceFilter');
    const clearFiltersBtn = document.getElementById('clearFiltersBtn');
    
    if (!logContainer) {
        console.warn('Log container bulunamadı');
        return;
    }
    
    console.log('📋 Log sistemi başlatılıyor...');
    
    // Log verileri ve state
    let currentPage = 1;
    let totalPages = 1;
    let totalLogs = 0;
    let allLogs = [];
    let filteredLogs = [];
    let autoRefreshActive = true;
    let refreshIntervalId = null;
    
    // Mevcut filtreler
    const currentFilters = {
        search: '',
        level: 'all',
        source: 'all'
    };

    // Pagination kontrollerini oluştur
    function createPaginationControls() {
        let paginationContainer = document.getElementById('paginationControls');
        
        if (!paginationContainer) {
            const paginationHTML = `
                <div id="paginationControls" class="pagination-controls">
                    <div class="pagination-info">
                        <span id="pageInfo">Sayfa 1 / 1</span>
                        <span class="log-count">Toplam: <span id="totalLogCount">0</span> kayıt</span>
                    </div>
                    <div class="pagination-buttons">
                        <button id="firstPageBtn" class="btn small" disabled>
                            <span>⏮️ İlk</span>
                        </button>
                        <button id="prevPageBtn" class="btn small" disabled>
                            <span>◀️ Önceki</span>
                        </button>
                        <span id="pageNumbers" class="page-numbers"></span>
                        <button id="nextPageBtn" class="btn small" disabled>
                            <span>Sonraki ▶️</span>
                        </button>
                        <button id="lastPageBtn" class="btn small" disabled>
                            <span>Son ⏭️</span>
                        </button>
                    </div>
                </div>
            `;
            
            logContainer.insertAdjacentHTML('afterend', paginationHTML);
            setupPaginationEventListeners();
        }
        
        updatePaginationDisplay();
    }

    // Pagination event listener'ları
    function setupPaginationEventListeners() {
        document.getElementById('firstPageBtn')?.addEventListener('click', () => goToPage(1));
        document.getElementById('prevPageBtn')?.addEventListener('click', () => goToPage(currentPage - 1));
        document.getElementById('nextPageBtn')?.addEventListener('click', () => goToPage(currentPage + 1));
        document.getElementById('lastPageBtn')?.addEventListener('click', () => goToPage(totalPages));
    }

    // Sayfa numaralarını oluştur
    function generatePageNumbers() {
        const pageNumbersContainer = document.getElementById('pageNumbers');
        if (!pageNumbersContainer) return;
        
        pageNumbersContainer.innerHTML = '';
        
        const maxVisible = 5;
        let start = Math.max(1, currentPage - 2);
        let end = Math.min(totalPages, start + maxVisible - 1);
        
        if (end - start < maxVisible - 1) {
            start = Math.max(1, end - maxVisible + 1);
        }
        
        if (start > 1) {
            const dots = document.createElement('span');
            dots.className = 'page-dots';
            dots.textContent = '...';
            pageNumbersContainer.appendChild(dots);
        }
        
        for (let i = start; i <= end; i++) {
            const pageBtn = document.createElement('button');
            pageBtn.className = `page-number-btn ${i === currentPage ? 'active' : ''}`;
            pageBtn.textContent = i;
            pageBtn.onclick = () => goToPage(i);
            pageNumbersContainer.appendChild(pageBtn);
        }
        
        if (end < totalPages) {
            const dots = document.createElement('span');
            dots.className = 'page-dots';
            dots.textContent = '...';
            pageNumbersContainer.appendChild(dots);
        }
    }

    // Pagination display güncelle
    function updatePaginationDisplay() {
        const pageInfo = document.getElementById('pageInfo');
        if (pageInfo) {
            pageInfo.textContent = `Sayfa ${currentPage} / ${totalPages}`;
        }
        
        const totalLogCountEl = document.getElementById('totalLogCount');
        if (totalLogCountEl) {
            totalLogCountEl.textContent = totalLogs;
        }
        
        const firstBtn = document.getElementById('firstPageBtn');
        const prevBtn = document.getElementById('prevPageBtn');
        const nextBtn = document.getElementById('nextPageBtn');
        const lastBtn = document.getElementById('lastPageBtn');
        
        if (firstBtn) firstBtn.disabled = currentPage <= 1;
        if (prevBtn) prevBtn.disabled = currentPage <= 1;
        if (nextBtn) nextBtn.disabled = currentPage >= totalPages;
        if (lastBtn) lastBtn.disabled = currentPage >= totalPages;
        
        generatePageNumbers();
    }

    // Belirli sayfaya git
    function goToPage(pageNumber) {
        if (pageNumber < 1 || pageNumber > totalPages || pageNumber === currentPage) {
            return;
        }
        
        currentPage = pageNumber;
        fetchLogs();
    }

    // Logları ekranda göster
    function renderLogs() {
        if (!logContainer) return;
        
        logContainer.innerHTML = '';
        
        if (filteredLogs.length === 0 && totalLogs === 0) {
            logContainer.innerHTML = `
                <div class="empty-state">
                    <div class="empty-icon">📋</div>
                    <h4>Log kaydı bulunamadı</h4>
                    <p>Henüz log kaydı yok. Sistem çalıştıkça loglar burada görünecek.</p>
                </div>
            `;
            return;
        } else if (filteredLogs.length === 0 && currentFilters.search) {
            logContainer.innerHTML = `
                <div class="empty-state">
                    <div class="empty-icon">🔍</div>
                    <h4>Arama sonucu bulunamadı</h4>
                    <p>Aradığınız kriterlere uygun log bulunamadı.</p>
                    <button class="btn secondary small" onclick="clearAllFilters()">🧹 Filtreleri Temizle</button>
                </div>
            `;
            return;
        }

        const fragment = document.createDocumentFragment();
        
        filteredLogs.forEach(log => {
            const logEntry = document.createElement('div');
            logEntry.className = `log-entry log-${log.l.toLowerCase()}`;
            
            let highlightedMessage = log.m;
            if (currentFilters.search) {
                const regex = new RegExp(`(${escapeRegExp(currentFilters.search)})`, 'gi');
                highlightedMessage = log.m.replace(regex, '<mark>$1</mark>');
            }
            
            logEntry.innerHTML = `
                <span class="log-time">${log.t}</span>
                <span class="log-level level-${log.l.toLowerCase()}">${log.l}</span>
                <span class="log-source">${log.s}</span>
                <span class="log-message">${highlightedMessage}</span>
            `;
            
            fragment.appendChild(logEntry);
        });
        
        logContainer.appendChild(fragment);
    }

    // İstatistikleri güncelle
    function updateLogStats() {
        updateElement('totalLogs', totalLogs.toString());
        updateElement('lastLogUpdate', new Date().toLocaleTimeString());
    }

    // Filtre badge güncelle
    function updateFilterBadges() {
        let activeFilterCount = 0;
        
        if (currentFilters.search) activeFilterCount++;
        if (currentFilters.level !== 'all') activeFilterCount++;
        if (currentFilters.source !== 'all') activeFilterCount++;
        
        if (clearFiltersBtn) {
            clearFiltersBtn.textContent = activeFilterCount > 0 ? 
                `🧹 Filtreleri Temizle (${activeFilterCount})` : 
                '🧹 Filtreleri Temizle';
            clearFiltersBtn.style.display = activeFilterCount > 0 ? 'block' : 'none';
        }
    }

    function escapeRegExp(string) {
        return string.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    }

    // Logları API'den çek
    async function fetchLogs() {
        if (state.logPaused) {
            console.log('Log yenileme duraklatıldı');
            return;
        }
        
        try {
            const params = new URLSearchParams({
                page: currentPage,
                level: currentFilters.level,
                source: currentFilters.source,
                search: currentFilters.search
            });
            
            const response = await secureFetch(`/api/logs?${params}`);
            if (response && response.ok) {
                const data = await response.json();
                
                currentPage = data.currentPage || 1;
                totalPages = data.totalPages || 1;
                totalLogs = data.totalLogs || 0;
                
                allLogs = data.logs || [];
                filteredLogs = allLogs;
                
                if (data.stats) {
                    updateElement('errorCount', data.stats.errorCount.toString());
                    updateElement('warningCount', data.stats.warnCount.toString());
                }
                
                updateSourceFilter();
                renderLogs();
                updateLogStats();
                updatePaginationDisplay();
                
                console.log(`✅ Sayfa ${currentPage}/${totalPages} - ${allLogs.length} log`);
            }
        } catch (error) {
            console.error('Log yükleme hatası:', error);
            showMessage('Log kayıtları yüklenemedi', 'error');
        }
    }

    // Kaynak filtresi güncelle
    function updateSourceFilter() {
        if (!logSourceFilter) return;
        
        const currentValue = logSourceFilter.value;
        const sources = new Set(['all']);
        
        allLogs.forEach(log => sources.add(log.s));
        
        logSourceFilter.innerHTML = '<option value="all">Tümü</option>';
        Array.from(sources).sort().forEach(source => {
            if (source !== 'all') {
                const option = document.createElement('option');
                option.value = source;
                option.textContent = source;
                if (source === currentValue) option.selected = true;
                logSourceFilter.appendChild(option);
            }
        });
    }

    // Yenileme interval ayarla
    function setRefreshInterval(interval) {
        if (refreshIntervalId) {
            clearInterval(refreshIntervalId);
        }
        
        if (autoRefreshActive && interval > 0) {
            refreshIntervalId = setInterval(() => {
                if (currentPage === 1 && !state.logPaused) {
                    fetchLogs();
                }
            }, interval);
            console.log(`⏱️ Otomatik yenileme: ${interval/1000}s`);
        }
    }

    // Global clear fonksiyonu
    window.clearAllFilters = function() {
        if (logSearch) logSearch.value = '';
        if (logLevelFilter) logLevelFilter.value = 'all';
        if (logSourceFilter) logSourceFilter.value = 'all';
        
        currentFilters.search = '';
        currentFilters.level = 'all';
        currentFilters.source = 'all';
        
        currentPage = 1;
        fetchLogs();
        showMessage('✅ Filtreler temizlendi', 'info');
    };

    // EVENT LISTENERS

    // Arama
    if (logSearch) {
        let searchTimeout;
        logSearch.addEventListener('input', (e) => {
            clearTimeout(searchTimeout);
            searchTimeout = setTimeout(() => {
                currentFilters.search = e.target.value.trim();
                currentPage = 1;
                fetchLogs();
            }, 300);
        });
    }

    // Seviye filtresi
    if (logLevelFilter) {
        logLevelFilter.addEventListener('change', (e) => {
            currentFilters.level = e.target.value;
            currentPage = 1;
            fetchLogs();
        });
    }

    // Kaynak filtresi
    if (logSourceFilter) {
        logSourceFilter.addEventListener('change', (e) => {
            currentFilters.source = e.target.value;
            currentPage = 1;
            fetchLogs();
        });
    }

    // Temizle filtreleri
    if (clearFiltersBtn) {
        clearFiltersBtn.addEventListener('click', window.clearAllFilters);
    }

    // Yenile butonu
    if (refreshLogsBtn) {
        refreshLogsBtn.addEventListener('click', () => {
            console.log('🔄 Manuel yenileme');
            fetchLogs();
        });
    }

    // Duraklat butonu
    if (pauseLogsBtn) {
        pauseLogsBtn.addEventListener('click', () => {
            state.logPaused = !state.logPaused;
            
            const btnIcon = pauseLogsBtn.querySelector('.btn-icon');
            const btnText = pauseLogsBtn.querySelector('.btn-text');
            
            if (state.logPaused) {
                btnIcon.textContent = '▶️';
                btnText.textContent = 'Devam Et';
                pauseLogsBtn.classList.add('paused');
                showMessage('⏸️ Log akışı duraklatıldı', 'info');
            } else {
                btnIcon.textContent = '⏸️';
                btnText.textContent = 'Duraklat';
                pauseLogsBtn.classList.remove('paused');
                showMessage('▶️ Log akışı devam ediyor', 'info');
                fetchLogs();
            }
        });
    }

    // TEMİZLE BUTONU - GERÇEKTEN SİLER
    if (clearLogsBtn) {
        clearLogsBtn.addEventListener('click', async () => {
            const confirmMsg = `⚠️ DİKKAT!\n\n${totalLogs} adet log kaydı KALICI olarak silinecek.\n\nBu işlem GERİ ALINAMAZ!\n\nDevam etmek istiyor musunuz?`;
            
            if (!confirm(confirmMsg)) return;
            if (!confirm("Son kez soruyorum: Emin misiniz?")) return;
            
            try {
                showMessage('🗑️ Loglar temizleniyor...', 'info');
                
                const response = await secureFetch('/api/logs/clear', { method: 'POST' });
                if (response && response.ok) {
                    const result = await response.json();
                    
                    allLogs = [];
                    filteredLogs = [];
                    currentPage = 1;
                    totalPages = 1;
                    totalLogs = 0;
                    
                    logContainer.innerHTML = `
                        <div class="empty-state">
                            <div class="empty-icon">✨</div>
                            <h4>Hafıza Temizlendi</h4>
                            <p>${result.previousCount} log kaydı kalıcı olarak silindi.</p>
                        </div>
                    `;
                    
                    updateLogStats();
                    updateFilterBadges();
                    updatePaginationDisplay();
                    
                    showMessage(`✅ ${result.previousCount} log hafızadan temizlendi`, 'success');
                    
                    setTimeout(() => {
                        fetchLogs();
                    }, 2000);
                } else {
                    showMessage('❌ Temizleme başarısız', 'error');
                }
            } catch (error) {
                console.error('Temizleme hatası:', error);
                showMessage('❌ Temizleme hatası', 'error');
            }
        });
    }

    // Export CSV
    if (exportLogsBtn) {
        exportLogsBtn.addEventListener('click', async () => {
            if (totalLogs === 0) {
                showMessage('⚠️ Dışa aktarılacak log yok', 'warning');
                return;
            }
            
            try {
                showMessage('📥 Loglar hazırlanıyor...', 'info');
                
                const allPagesLogs = [];
                for (let page = 1; page <= totalPages; page++) {
                    const params = new URLSearchParams({ page: page });
                    const response = await secureFetch(`/api/logs?${params}`);
                    if (response && response.ok) {
                        const data = await response.json();
                        allPagesLogs.push(...(data.logs || []));
                    }
                }
                
                const BOM = '\uFEFF';
                let csvContent = 'sep=;\n';
                csvContent += '"Zaman";"Seviye";"Kaynak";"Mesaj"\n';
                
                allPagesLogs.forEach(log => {
                    const cleanMessage = log.m.replace(/"/g, '""').replace(/[\r\n\t]/g, ' ').trim();
                    csvContent += `"${log.t}";"${log.l}";"${log.s}";"${cleanMessage}"\n`;
                });
                
                const blob = new Blob([BOM + csvContent], { type: 'text/csv;charset=utf-8' });
                const now = new Date();
                const filename = `logs_${now.toISOString().slice(0, 10)}_${now.toTimeString().slice(0, 5).replace(':', '')}.csv`;
                
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = filename;
                document.body.appendChild(a);
                a.click();
                document.body.removeChild(a);
                URL.revokeObjectURL(url);
                
                showMessage(`✅ ${allPagesLogs.length} log CSV olarak indirildi`, 'success');
                
            } catch (error) {
                console.error('Export hatası:', error);
                showMessage('❌ Export hatası', 'error');
            }
        });
    }

    // Export Excel
    if (exportExcelBtn) {
        exportExcelBtn.addEventListener('click', async () => {
            if (totalLogs === 0) {
                showMessage('⚠️ Dışa aktarılacak log yok', 'warning');
                return;
            }
            
            try {
                showMessage('📊 Excel hazırlanıyor...', 'info');
                
                const allPagesLogs = [];
                for (let page = 1; page <= totalPages; page++) {
                    const params = new URLSearchParams({ page: page });
                    const response = await secureFetch(`/api/logs?${params}`);
                    if (response && response.ok) {
                        const data = await response.json();
                        allPagesLogs.push(...(data.logs || []));
                    }
                }
                
                // Excel XML format
                let xml = '<?xml version="1.0" encoding="UTF-8"?>\n';
                xml += '<Workbook xmlns="urn:schemas-microsoft-com:office:spreadsheet"\n';
                xml += ' xmlns:ss="urn:schemas-microsoft-com:office:spreadsheet">\n';
                xml += '<Worksheet ss:Name="Logs">\n<Table>\n';
                
                // Header
                xml += '<Row>\n';
                xml += '<Cell><Data ss:Type="String">Zaman</Data></Cell>\n';
                xml += '<Cell><Data ss:Type="String">Seviye</Data></Cell>\n';
                xml += '<Cell><Data ss:Type="String">Kaynak</Data></Cell>\n';
                xml += '<Cell><Data ss:Type="String">Mesaj</Data></Cell>\n';
                xml += '</Row>\n';
                
                // Data
                allPagesLogs.forEach(log => {
                    xml += '<Row>\n';
                    xml += `<Cell><Data ss:Type="String">${escapeXml(log.t)}</Data></Cell>\n`;
                    xml += `<Cell><Data ss:Type="String">${escapeXml(log.l)}</Data></Cell>\n`;
                    xml += `<Cell><Data ss:Type="String">${escapeXml(log.s)}</Data></Cell>\n`;
                    xml += `<Cell><Data ss:Type="String">${escapeXml(log.m)}</Data></Cell>\n`;
                    xml += '</Row>\n';
                });
                
                xml += '</Table>\n</Worksheet>\n</Workbook>';
                
                function escapeXml(str) {
                    return str.replace(/[<>&'"]/g, c => {
                        switch(c) {
                            case '<': return '&lt;';
                            case '>': return '&gt;';
                            case '&': return '&amp;';
                            case "'": return '&apos;';
                            case '"': return '&quot;';
                        }
                    });
                }
                
                const blob = new Blob(['\uFEFF' + xml], { type: 'application/vnd.ms-excel;charset=utf-8' });
                const now = new Date();
                const filename = `logs_${now.toISOString().slice(0, 10)}.xls`;
                
                const url = URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = filename;
                document.body.appendChild(a);
                a.click();
                document.body.removeChild(a);
                URL.revokeObjectURL(url);
                
                showMessage(`✅ ${allPagesLogs.length} log Excel olarak indirildi`, 'success');
                
            } catch (error) {
                console.error('Excel export hatası:', error);
                showMessage('❌ Excel export hatası', 'error');
            }
        });
    }

    // Otomatik yenileme toggle
    if (autoRefreshToggle) {
        autoRefreshToggle.addEventListener('click', () => {
            autoRefreshActive = !autoRefreshActive;
            
            autoRefreshToggle.setAttribute('data-active', autoRefreshActive.toString());
            autoRefreshToggle.classList.toggle('active', autoRefreshActive);
            
            const toggleIcon = autoRefreshToggle.querySelector('.toggle-icon');
            const toggleText = autoRefreshToggle.querySelector('.toggle-text');
            
            if (autoRefreshActive) {
                toggleIcon.textContent = '🔄';
                toggleText.textContent = 'Otomatik Yenileme';
                showMessage('🔄 Otomatik yenileme açık', 'info');
                
                const interval = parseInt(refreshInterval?.value || '5000');
                setRefreshInterval(interval);
            } else {
                toggleIcon.textContent = '⏸️';
                toggleText.textContent = 'Manuel Yenileme';
                showMessage('⏸️ Otomatik yenileme kapalı', 'info');
                
                setRefreshInterval(0);
            }
        });
    }

    // Yenileme aralığı
    if (refreshInterval) {
        refreshInterval.addEventListener('change', () => {
            if (autoRefreshActive) {
                const interval = parseInt(refreshInterval.value);
                setRefreshInterval(interval);
                
                const text = refreshInterval.options[refreshInterval.selectedIndex].text;
                showMessage(`⏱️ Yenileme: ${text}`, 'info');
            }
        });
    }

    // Helper function
    function updateElement(id, value) {
        const el = document.getElementById(id);
        if (el) el.textContent = value;
    }

    // BAŞLATMA
    
    // Pagination oluştur
    createPaginationControls();
    
    // İlk yükleme
    fetchLogs();
    
    // Otomatik yenileme
    const initialInterval = parseInt(refreshInterval?.value || '5000');
    setRefreshInterval(initialInterval);
    
    // Cleanup
    window.addEventListener('beforeunload', () => {
        if (refreshIntervalId) {
            clearInterval(refreshIntervalId);
        }
    });
    
    console.log('✅ Log sistemi hazır (Pagination destekli, otomatik kaydırma yok)');
}

// Yedekleme Sayfası
function initBackupPage() {
    // Download butonu için event listener ekle
    const downloadBtn = document.getElementById('downloadBackupBtn');
    if (downloadBtn) {
        downloadBtn.addEventListener('click', downloadBackup);
    }
    
    // Upload form event listener
    document.getElementById('uploadBackupForm')?.addEventListener('submit', async (e) => {
        e.preventDefault();
        const fileInput = document.getElementById('backupFile');
        if (fileInput.files.length === 0) {
            showMessage('Lütfen bir yedek dosyası seçin.', 'warning');
            return;
        }
        const formData = new FormData();
        formData.append('backup', fileInput.files[0]);
        
        showMessage('Yedek yükleniyor, lütfen bekleyin. Cihaz işlem sonrası yeniden başlatılacak.', 'info');

        try {
            const response = await secureFetch('/api/backup/upload', {
                method: 'POST',
                body: formData
            });

            if(response && response.ok){
                showMessage('Yedek başarıyla yüklendi! Cihaz 3 saniye içinde yeniden başlatılıyor...', 'success');
                setTimeout(() => window.location.href = '/', 3000);
            } else {
                showMessage('Yedek yükleme başarısız oldu. Dosyanın geçerli olduğundan emin olun.', 'error');
            }
        } catch (error) {
            console.error('Backup yükleme hatası:', error);
            showMessage('Bir hata oluştu', 'error');
        }
    });
}

// Yedek indirme fonksiyonu (global olarak tanımlanmalı - window nesnesine ekle)
window.downloadBackup = async function downloadBackup() {
    try {
        const response = await secureFetch('/api/backup/download');
        
        if (response && response.ok) {
            // Blob olarak indirme
            const blob = await response.blob();
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `teias_eklim_backup_${new Date().toISOString().slice(0, 10)}.json`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            window.URL.revokeObjectURL(url);
            
            showMessage('Yedek dosyası indiriliyor...', 'success');
        } else {
            showMessage('Yedek indirme yetkisi yok veya bir hata oluştu', 'error');
        }
    } catch (error) {
        console.error('Backup indirme hatası:', error);
        showMessage('Yedek indirilirken bir hata oluştu', 'error');
    }
}

    // --- 3. SAYFA YÖNLENDİRİCİ (ROUTER) İÇİN SAYFA LİSTESİ ---
    const pages = {
        dashboard: { file: 'pages/dashboard.html', init: initDashboardPage },
        network: { file: 'pages/network.html', init: initNetworkPage },
        ntp: { file: 'pages/ntp.html', init: initNtpPage },
        baudrate: { file: 'pages/baudrate.html', init: initBaudRatePage },
        fault: { file: 'pages/fault.html', init: initFaultPage },
        log: { file: 'pages/log.html', init: initLogPage },
        datetime: { file: 'pages/datetime.html', init: initDateTimePage }, // YENİ EKLENDİ
        systeminfo: { file: 'pages/systeminfo.html', init: initSystemInfoPage },
        account: { file: 'pages/account.html', init: initAccountPage },
        backup: { file: 'pages/backup.html', init: initBackupPage }
    };

    // --- 4. TEMEL FONKSİYONLAR (Router, Auth, API Fetch) ---

    function logout() {
        Object.values(state.pollingIntervals).forEach(clearInterval);
        localStorage.removeItem('sessionToken');
        window.location.href = '/login.html';
    }

    async function secureFetch(url, options = {}) {
        if (!state.token) {
            logout();
            return null;
        }
        const headers = { ...options.headers, 'Authorization': `Bearer ${state.token}` };
        if (options.body instanceof FormData) {
             delete headers['Content-Type'];
        }

        try {
            const response = await fetch(url, { ...options, headers });
            if (response.status === 401) {
                logout();
                return null;
            }
            return response;
        } catch (error) {
            console.error('API İsteği Hatası:', error);
            updateElement('currentDateTime', 'Bağlantı Hatası');
            return null;
        }
    }

    async function loadPage(pageName) {
        Object.values(state.pollingIntervals).forEach(clearInterval);

        const page = pages[pageName] || pages['dashboard'];
        const mainContent = document.getElementById('main-content');
        mainContent.innerHTML = '<div class="loading-container"><div class="loading-spinner"></div><p>Yükleniyor...</p></div>';

        try {
            const response = await secureFetch(`/${page.file}`);
            if (response && response.ok) {
                mainContent.innerHTML = await response.text();
                document.querySelectorAll('.nav-item').forEach(link => {
                    link.classList.toggle('active', link.dataset.page === pageName);
                });
                if (page.init) {
                    try {
                        page.init();
                    } catch(e) {
                        console.error("Sayfa başlatma hatası:", e);
                        mainContent.innerHTML = `<div class="error">Sayfa başlatılırken bir hata oluştu.</div>`;
                    }
                }
                // Bildirim sayısını güncelle
                updateNotificationCount();
            } else {
                mainContent.innerHTML = `<div class="error">Sayfa yüklenemedi (Hata: ${response ? response.status : 'Ağ Hatası'})</div>`;
            }
        } catch (error) {
            console.error('Sayfa yükleme hatası:', error);
            mainContent.innerHTML = `<div class="error">Sayfa yüklenirken bir hata oluştu.</div>`;
        }
    }

    function router() {
        const pageName = window.location.hash.substring(1) || 'dashboard';
        loadPage(pageName);
    }

    // --- 5. YARDIMCI UI FONKSİYONLARI ---
    
    function appendLog(logData) {
        const logContainer = document.getElementById('logContainer');
        if (!logContainer) return;

        const logEntry = document.createElement('div');
        logEntry.className = `log-entry log-${logData.l.toLowerCase()}`;
        logEntry.innerHTML = `
            <span class="log-time">${logData.t}</span>
            <span class="log-level">${logData.l}</span>
            <span class="log-source">${logData.s}</span>
            <span class="log-message">${logData.m}</span>`;
        logContainer.appendChild(logEntry);
        if (state.autoScroll) logContainer.scrollTop = logContainer.scrollHeight;
    }

    function updateDashboardUI(data) {
        updateElement('currentDateTime', data.datetime);
        const ethStatusEl = document.getElementById('ethernetStatus');
        if(ethStatusEl) ethStatusEl.innerHTML = `<span class="status-indicator ${data.ethernetStatus ? 'active' : 'error'}"></span> ${data.ethernetStatus ? 'Bağlı' : 'Yok'}`;
        const timeStatusEl = document.getElementById('ntpStatus');
        if(timeStatusEl) timeStatusEl.innerHTML = `<span class="status-indicator ${data.timeSynced ? 'active' : 'warning'}"></span> ${data.timeSynced ? 'Senkronize' : 'Bekleniyor'}`;
        
        updateElement('deviceName', data.deviceName);
        updateElement('tmName', data.tmName);
        updateElement('deviceIP', data.deviceIP);
        updateElement('uptime', data.uptime);
        
        const memoryUsage = document.getElementById('memoryUsage');
        if(memoryUsage && data.freeHeap && data.totalHeap) {
            const usagePercent = Math.round(((data.totalHeap - data.freeHeap) / data.totalHeap) * 100);
            const progressBar = memoryUsage.querySelector('.progress-fill');
            const percentText = memoryUsage.querySelector('span:last-child');
            if(progressBar) progressBar.style.width = `${usagePercent}%`;
            if(percentText) percentText.textContent = `${usagePercent}%`;
        }
    }

    function updateElement(id, value, width = null) {
        const element = document.getElementById(id);
        if (element) {
            if (width !== null) {
                element.style.width = width + '%';
            } else {
                element.textContent = value;
            }
        }
    }

function showMessage(text, type = 'info', duration = 4000) {
    // Önce normal container'ı dene
    let container = document.getElementById('message-container');
    
    // Eğer yoksa, body'nin sonuna ekle
    if (!container) {
        container = document.createElement('div');
        container.id = 'message-container';
        container.style.cssText = `
            position: fixed;
            top: 20px;
            right: 20px;
            z-index: 10000;
            max-width: 400px;
        `;
        document.body.appendChild(container);
    }
    
    const messageDiv = document.createElement('div');
    messageDiv.className = `message ${type}`;
    messageDiv.style.cssText = `
        padding: 12px 20px;
        margin-bottom: 10px;
        border-radius: 8px;
        animation: slideInRight 0.3s ease-out;
        background: ${type === 'error' ? '#f56565' : type === 'success' ? '#48bb78' : type === 'warning' ? '#ed8936' : '#4299e1'};
        color: white;
        box-shadow: 0 4px 6px rgba(0,0,0,0.1);
    `;
    messageDiv.textContent = text;
    
    container.appendChild(messageDiv);
    
    setTimeout(() => {
        messageDiv.style.animation = 'slideOutRight 0.3s ease-out';
        setTimeout(() => {
            if (messageDiv.parentNode) {
                messageDiv.parentNode.removeChild(messageDiv);
            }
        }, 300);
    }, duration);
    
    console.log(`[${type.toUpperCase()}] ${text}`);
}

    // Notification sistemi
    async function updateNotificationCount() {
        try {
            const response = await secureFetch('/api/notifications');
            if (response && response.ok) {
                const data = await response.json();
                const badge = document.getElementById('notificationCount');
                if (badge) {
                    badge.textContent = data.count;
                    badge.style.display = data.count > 0 ? 'block' : 'none';
                }
            }
        } catch (error) {
            console.error('Bildirim hatası:', error);
        }
    }

    // Yardımcı formatters
    function formatBytes(bytes) {
        if (bytes === 0) return '0 B';
        const k = 1024;
        const sizes = ['B', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    function formatUptime(seconds) {
        const days = Math.floor(seconds / 86400);
        const hours = Math.floor((seconds % 86400) / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        const secs = seconds % 60;
        
        if (days > 0) {
            return `${days}g ${hours}s ${minutes}d`;
        } else if (hours > 0) {
            return `${hours}s ${minutes}d ${secs}s`;
        } else {
            return `${minutes}d ${secs}s`;
        }
    }

    // --- LED PANEL FONKSİYONLARI - Dashboard sayfasına taşındı ---
    // Tüm LED panel fonksiyonları initDashboardPage() içinde
    // initLedPanelForDashboard() fonksiyonunda toplanmıştır (satır 82-205)

    // --- 6. UYGULAMA BAŞLATMA ---
    function main() {
        // Login veya parola değiştirme sayfasındaysak ana scripti çalıştırma
        if (window.location.pathname.includes('login.html') || window.location.pathname.includes('password_change.html')) {
            return; 
        }

        // Token yoksa login sayfasına yönlendir
        if (!state.token) {
            logout();
            return;
        }
        
        // Device info'yu al ve mDNS adresini göster
        fetch('/api/device-info')
            .then(r => r.json())
            .then(data => {
                updateElement('mdnsAddress', data.mdns || 'teias-eklim.local');
            })
            .catch(() => {
                updateElement('mdnsAddress', 'teias-eklim.local');
            });
        
        // Çıkış butonu
        document.getElementById('logoutBtn')?.addEventListener('click', (e) => { 
            e.preventDefault(); 
            logout(); 
        });
        
        // Navigasyon menüsü
        document.querySelectorAll('.nav-item').forEach(link => {
            link.addEventListener('click', (e) => {
                e.preventDefault();
                window.location.hash = link.dataset.page;
            });
        });
        
        // Notification butonu
        document.getElementById('notificationBtn')?.addEventListener('click', async () => {
            const response = await secureFetch('/api/notifications');
            if (response && response.ok) {
                const data = await response.json();
                console.log('Bildirimler:', data);
                // TODO: Bildirim popup'ı göster
            }
        });

        // Session keepalive başlat
        startSessionKeepalive();
        
        // Bildirim güncelleme timer'ı
        setInterval(updateNotificationCount, 30000); // 30 saniyede bir
        
        // Router'ı dinle ve ilk sayfayı yükle
        window.addEventListener('hashchange', router);
        router();
    }

    main();
});