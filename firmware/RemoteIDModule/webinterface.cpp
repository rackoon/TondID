#include "webinterface.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <WiFiAP.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <esp_system.h>
#include <opendroneid.h>
#include "parameters.h"
#include "romfs.h"
#include "check_firmware.h"
#include "status.h"

static WebServer server(80);

static void randomise_mock_profile(void)
{
    static const struct {
        uint8_t ua_type;
        const char *self_id;
    } profiles[] = {
        { ODID_UATYPE_HELICOPTER_OR_MULTIROTOR, "QUAD TEST" },
        { ODID_UATYPE_AEROPLANE, "FIXED TEST" },
        { ODID_UATYPE_HELICOPTER_OR_MULTIROTOR, "HEXA TEST" },
        { ODID_UATYPE_HYBRID_LIFT, "VTOL TEST" },
        { ODID_UATYPE_GYROPLANE, "GYRO TEST" },
    };

    const auto &profile = profiles[esp_random() % (sizeof(profiles) / sizeof(profiles[0]))];
    char uas_id[21] {};
    char operator_id[21] {};
    snprintf(uas_id, sizeof(uas_id), "1596%04X%04X%04X",
             unsigned(esp_random() & 0xFFFF),
             unsigned(esp_random() & 0xFFFF),
             unsigned(esp_random() & 0xFFFF));
    snprintf(operator_id, sizeof(operator_id), "EST%08lX%05u",
             (unsigned long)esp_random(),
             unsigned(esp_random() % 100000));

    g.set_by_name_uint8("MOCK_ENABLE", 1);
    g.set_by_name_uint8("UAS_TYPE", profile.ua_type);
    g.set_by_name_uint8("UAS_ID_TYPE", ODID_IDTYPE_SERIAL_NUMBER);
    g.set_by_name_string("UAS_ID", uas_id);
    g.set_by_name_string("MOCK_SELF_ID", profile.self_id);
    g.set_by_name_string("MOCK_OP_ID", operator_id);
}

static void apply_mock_arg(const char *arg_name, const char *param_name)
{
    if (server.hasArg(arg_name)) {
        g.set_by_name_string(param_name, server.arg(arg_name).c_str());
    }
}

static void apply_qa_arg(const char *arg_name, const char *param_name)
{
    if (server.hasArg(arg_name)) {
        g.set_by_name_string(param_name, server.arg(arg_name).c_str());
    }
}

/*
  serve files from ROMFS
 */
class ROMFS_Handler : public RequestHandler
{
    bool canHandle(HTTPMethod method, String uri) {
        if (uri == "/") {
            uri = "/index.html";
        }
        uri = "web" + uri;
        if (ROMFS::exists(uri.c_str())) {
            return true;
        }
        return false;
    }

    bool handle(WebServer& server, HTTPMethod requestMethod, String requestUri) {
        if (requestUri == "/") {
            requestUri = "/index.html";
        }
        String uri = "web" + requestUri;
        Serial.printf("handle: '%s'\n", requestUri.c_str());

        // work out content type
        const char *content_type = "text/html";
        const struct {
            const char *extension;
            const char *content_type;
        } extensions[] = {
            { ".js", "text/javascript" },
            { ".jpg", "image/jpeg" },
            { ".png", "image/png" },
            { ".svg", "image/svg+xml" },
            { ".css", "text/css" },
        };
        for (const auto &e : extensions) {
            if (uri.endsWith(e.extension)) {
                content_type = e.content_type;
                break;
            }
        }

        auto *f = ROMFS::find_stream(uri.c_str());
        if (f != nullptr) {
            server.sendHeader("Content-Encoding", "gzip");
            server.streamFile(*f, content_type);
            delete f;
            return true;
        }
        return false;
    }

} ROMFS_Handler;

/*
  serve files from ROMFS
 */
class AJAX_Handler : public RequestHandler
{
    bool canHandle(HTTPMethod method, String uri) {
        return uri == "/ajax/status.json";
    }

    bool handle(WebServer& server, HTTPMethod requestMethod, String requestUri) {
        if (requestUri != "/ajax/status.json") {
            return false;
        }
        server.send(200, "application/json", status_json());
        return true;
    }

} AJAX_Handler;

/*
  init web server
 */
void WebInterface::init(void)
{
    Serial.printf("WAP start %s %s\n", g.wifi_ssid, g.wifi_password);
    IPAddress myIP = WiFi.softAPIP();

    server.addHandler( &AJAX_Handler );
    server.addHandler( &ROMFS_Handler );

    server.on("/ajax/mock/status.json", HTTP_GET, []() {
        server.send(200, "application/json", mock_json());
    });

    server.on("/ajax/qa/status.json", HTTP_GET, []() {
        server.send(200, "application/json", qa_json());
    });

    server.on("/ajax/mock/set", HTTP_GET, []() {
        apply_mock_arg("enable", "MOCK_ENABLE");
        apply_mock_arg("home_lat", "MOCK_HOME_LAT");
        apply_mock_arg("home_lon", "MOCK_HOME_LON");
        apply_mock_arg("ghosts", "MOCK_GHOSTS");
        apply_mock_arg("radius", "MOCK_RADIUS");
        apply_mock_arg("speed", "MOCK_SPEED");
        apply_mock_arg("uas_type", "UAS_TYPE");
        apply_mock_arg("uas_id", "UAS_ID");
        apply_mock_arg("self_id", "MOCK_SELF_ID");
        apply_mock_arg("operator_id", "MOCK_OP_ID");
        server.send(200, "application/json", mock_json());
    });

    server.on("/ajax/mock/randomize", HTTP_GET, []() {
        randomise_mock_profile();
        server.send(200, "application/json", mock_json());
    });

    server.on("/ajax/qa/set", HTTP_GET, []() {
        apply_qa_arg("enable", "QA_ENABLE");
        apply_qa_arg("uas_id_seed", "QA_UAS_SEED");
        apply_qa_arg("home_lat", "QA_HOME_LAT");
        apply_qa_arg("home_lon", "QA_HOME_LON");
        apply_qa_arg("alt_m", "QA_ALT_M");
        apply_qa_arg("radius", "QA_RADIUS");
        apply_qa_arg("speed", "QA_SPEED");
        apply_qa_arg("heading_mode", "QA_HEADING");
        apply_qa_arg("slot_count", "QA_SLOTS");
        apply_qa_arg("lab_label", "QA_LABEL");
        apply_qa_arg("lab_mac_override", "QA_LAA_MAC");
        server.send(200, "application/json", qa_json());
    });

    /*handling uploading firmware file */
    server.on("/update", HTTP_POST, []() {
        if (Update.hasError()) {
			server.sendHeader("Connection", "close");
		    server.send(500, "text/plain","FAIL");
		    Serial.printf("Update Failed: Update function has errors\n");
		    delay(5000);
		} else {
			server.sendHeader("Connection", "close");
			server.send(200, "text/plain","OK");
			Serial.printf("Update Success: \nRebooting...\n");
			delay(1000);
			ESP.restart();
		}
    }, [this]() {
        HTTPUpload& upload = server.upload();
        static const esp_partition_t* partition_new_firmware = esp_ota_get_next_update_partition(NULL); //get OTA partion to which we will write new firmware file;
        if (upload.status == UPLOAD_FILE_START) {
            Serial.printf("Update: %s\n", upload.filename.c_str());
            lead_len = 0;

            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            /* flashing firmware to ESP*/
            if (lead_len < sizeof(lead_bytes)) {
                uint32_t n = sizeof(lead_bytes)-lead_len;
                if (n > upload.currentSize) {
                    n = upload.currentSize;
                }
                memcpy(&lead_bytes[lead_len], upload.buf, n);
                lead_len += n;
            }
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            // write extra bytes to force flush of the buffer before we check signature
            uint32_t extra = SPI_FLASH_SEC_SIZE+1;
            while (extra--) {
                uint8_t ff = 0xff;
                Update.write(&ff, 1);
            }
            if (!CheckFirmware::check_OTA_next(partition_new_firmware, lead_bytes, lead_len)) {
                Serial.printf("Update Failed: firmware checks have errors\n");
                server.sendHeader("Connection", "close");
                server.send(500, "text/plain","FAIL");
                delay(5000);
            } else if (Update.end(true)) {
                Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
                server.sendHeader("Connection", "close");
                server.send(200, "text/plain","OK");
            } else {
                Update.printError(Serial);
                Serial.printf("Update Failed: Update.end function has errors\n");
                server.sendHeader("Connection", "close");
                server.send(500, "text/plain","FAIL");
                delay(5000);
            }
        }
    });
    Serial.printf("WAP started\n");
    server.begin();
}

void WebInterface::update()
{
    if (!initialised) {
        init();
        initialised = true;
    }
    server.handleClient();
}
