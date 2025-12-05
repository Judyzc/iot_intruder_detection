/* 
Web app for ESP32-S3 showing camera stream.
*/

#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include "driver/ledc.h"
#include "sdkconfig.h"
#include "camera_index.h"
#include "hardware_control.h"
#include "intruder_task.h"
#include "face_state.h"
#include "FS.h"
#include "SPIFFS.h"
#include <WiFi.h>
#include <WiFiClient.h> 
#include <HTTPClient.h>
#define TAG "app: "

/* all part of face detection and recogition */
#include <vector>
#include <list>
#include <cstdarg> 

// First Stage Detector - Multi State Regression 1 - Run lightweight model to find potential face candidate regions
// Outputs a bounding box
// (score_threshold, nms_threshold, top_k, center_variance)
#include "human_face_detect_msr01.hpp" 
// Second Stage Detector - Multi Network Pipeline - Takes bounding boxes and improves location and add facial landmarks
// Needed for proper landmark alignment -- two stage recognition
#include "human_face_detect_mnp01.hpp"

#define TWO_STAGE 1                         // 1: detect by two-stage, uses keypoints, needed for facial recognition
#include "face_recognition_tool.hpp"
// Espressif face recognition models
#include "face_recognition_112_v1_s8.hpp"   // less accurate but runs faster - what we're using!

// Max number of enrolled faces
#define FACE_ID_SAVE_NUMBER 7

// stream resolution variables
#define FACE_COLOR_WHITE 0x00FFFFFF
#define FACE_COLOR_BLACK 0x00000000
#define FACE_COLOR_RED 0x000000FF
#define FACE_COLOR_GREEN 0x0000FF00
#define FACE_COLOR_BLUE 0x00FF0000
#define FACE_COLOR_YELLOW (FACE_COLOR_RED | FACE_COLOR_GREEN)
#define FACE_COLOR_CYAN (FACE_COLOR_BLUE | FACE_COLOR_GREEN)
#define FACE_COLOR_PURPLE (FACE_COLOR_BLUE | FACE_COLOR_RED)
#define ENROLL_INTERVAL_MS 5000          // 5 seconds between enroll captures (tune if you like)

// Enables live streaming over HTTP by mix of JPEG to MJPEG though mixed
#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

/* Two httpd instances (servers) with different ports: one for control/capture (port X) and one for streaming (port X+1).  */
httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

/* face detection and recognition variables */
volatile bool detection_via_gui = false;
volatile bool recognition_via_gui = false;
volatile bool detection_via_pir = false;
volatile bool recognition_via_pir = false;

volatile int8_t is_enrolling = 0;
volatile int8_t detection_enabled = 0;
volatile int8_t recognition_enabled = 0;

// Delay showing the enrollment messages on screen
static bool show_enroll_msg = false;
static char enroll_msg_text[64];
static int64_t enroll_msg_until_us = 0;
#define ENROLL_MSG_DURATION_MS 3000

/* 
    face recognition model (neural network) 
    extracts face embeddings and compares to IDs
    enrolls new IDs
    loads/saves IDs to flash
    (112x112) aligned face images with 8 bit signed weights and activations
*/
FaceRecognition112V1S8 recognizer;

// ----- FUNCTIONS --------------------------------

/* Prints out text at top of frame buffer (intruder, id, confidence, etc)*/
static void rgb_print(fb_data_t *fb, uint32_t color, const char *str)
{
    fb_gfx_print(fb, (fb->width - (strlen(str) * 14)) / 2, 10, color, str);
}


/* Prints out ID and confidence values onto screen */
static int rgb_printf(fb_data_t *fb, uint32_t color, const char *format, ...)
{
    char loc_buf[64];
    char *temp = loc_buf;
    int len;
    va_list arg;
    va_list copy;
    va_start(arg, format);
    va_copy(copy, arg);
    len = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (len < 0) {
        va_end(arg);
        return 0;
    }
    if (len >= (int)sizeof(loc_buf)) {
        temp = (char *)malloc(len + 1);
        if (temp == NULL) {
            va_end(arg);
            return 0;
        }
    }
    vsnprintf(temp, (size_t)len + 1, format, arg);
    va_end(arg);
    rgb_print(fb, color, temp);
    if (temp != loc_buf) {
        free(temp);
    }
    return len;
}


/*Draw rectangles and 5 landmark indicators for detected face*/
static void draw_face_boxes(fb_data_t *fb, std::list<dl::detect::result_t> *results, int face_id)
{
    int x, y, w, h;
    uint32_t color = FACE_COLOR_YELLOW;
    if (face_id < 0)
    {
        color = FACE_COLOR_RED;
    }
    else if (face_id > 0)
    {
        color = FACE_COLOR_GREEN;
    }
    if(fb->bytes_per_pixel == 2){
        color = ((color >> 16) & 0x001F) | ((color >> 3) & 0x07E0) | ((color << 8) & 0xF800);
    }
    int i = 0;
    for (std::list<dl::detect::result_t>::iterator prediction = results->begin(); prediction != results->end(); prediction++, i++)
    {
        // rectangle box
        x = (int)prediction->box[0];
        y = (int)prediction->box[1];
        w = (int)prediction->box[2] - x + 1;
        h = (int)prediction->box[3] - y + 1;
        if((x + w) > fb->width){
            w = fb->width - x;
        }
        if((y + h) > fb->height){
            h = fb->height - y;
        }
        fb_gfx_drawFastHLine(fb, x, y, w, color);
        fb_gfx_drawFastHLine(fb, x, y + h - 1, w, color);
        fb_gfx_drawFastVLine(fb, x, y, h, color);
        fb_gfx_drawFastVLine(fb, x + w - 1, y, h, color);
        // landmarks (left eye, mouth left, nose, right eye, mouth right)
        int x0, y0, j;
        for (j = 0; j < 10; j+=2) {
            x0 = (int)prediction->keypoint[j];
            y0 = (int)prediction->keypoint[j+1];
            fb_gfx_fillRect(fb, x0, y0, 3, 3, color);
        }
    }
}


/*
    Runs facial recognition
    Takes detected face and keypoints - converts into tensor to put into the NN    
*/
static int run_face_recognition(fb_data_t *fb, std::list<dl::detect::result_t> *results)
{
    std::vector<int> landmarks = results->front().keypoint;
    int id = -1;
    // Turns framebuffer into a Tensor object to put into the NN
    Tensor<uint8_t> tensor;
    tensor.set_element((uint8_t *)fb->data).set_shape({fb->height, fb->width, 3}).set_auto_free(false);
    // If enrolling, a face, check how many faces can be enrolled
    int enrolled_count = recognizer.get_enrolled_id_num();
    bool did_enroll = false;
    if (enrolled_count < FACE_ID_SAVE_NUMBER && is_enrolling) {
        // timestamp of last enroll, 5 second intervals for enrolling
        static int64_t last_enroll_time_us = 0;
        int64_t now_us = esp_timer_get_time();
        if (now_us - last_enroll_time_us >= (int64_t)ENROLL_INTERVAL_MS * 1000) {
            id = recognizer.enroll_id(tensor, landmarks, "", true);
            last_enroll_time_us = now_us;
            did_enroll = true;
            ESP_LOGI(TAG, "Enrolled ID: %d", id);
            // delay a little to slow down enrolling and verify that identity has been enrolled
            sprintf(enroll_msg_text, "ID[%u] enrolled", id);
            show_enroll_msg = true;
            enroll_msg_until_us = esp_timer_get_time() + (int64_t)ENROLL_MSG_DURATION_MS * 1000;
            rgb_printf(fb, FACE_COLOR_CYAN, enroll_msg_text);
            return id;
        }
    }

    face_info_t recognize = recognizer.recognize(tensor, landmarks);
    if(!is_enrolling) {
        if (recognize.id >= 0) {
            // recognized owner — print single green line
            rgb_printf(fb, FACE_COLOR_GREEN, "ID[%u]: %.2f", recognize.id, recognize.similarity);
            // log data
            send_to_database(false, recognize.id, recognize.similarity);
        } else {
            // intruder — single red message
            rgb_print(fb, FACE_COLOR_RED, "Intruder Alert!");
            intruder_queue_send(1);
            // Serial.println("INTRUDER");
            // log data
            send_to_database(true, -1, recognize.similarity);
        }
    }
    return recognize.id;
}


/* Used to check either flag and determine if recognition or detection has been triggered by gui or pir */
void recompute_face_state() {
    // recognition depends on detection
    // If no face has been enrolled yet, ignore PIR-sourced requests to enable detection/recognition.
    int enrolled_count = recognizer.get_enrolled_id_num();
    if (enrolled_count == 0) {
        if (detection_via_pir || recognition_via_pir) {
            ESP_LOGI("face_state", "PIR requested detection/recognition but no enrolled faces - ignoring PIR");
        }
        // Prevent PIR from enabling detection/recognition when there are no enrolled faces.
        detection_via_pir = false;
        recognition_via_pir = false;
    }
    // recognition depends on detection
    recognition_enabled = (recognition_via_gui || recognition_via_pir) ? 1 : 0;
    detection_enabled = (detection_via_gui || detection_via_pir || is_enrolling) ? 1 : 0;
    if (recognition_enabled) detection_enabled = 1;
    Serial.println("recomputing");
    ESP_LOGI("face_state", "recompute: d_gui=%d d_pir=%d enroll=%d => detect=%d | r_gui=%d r_pir=%d => recog=%d",
             detection_via_gui, detection_via_pir, is_enrolling,
             detection_enabled, recognition_via_gui, recognition_via_pir, recognition_enabled);
}


/* Live MJPEG Face Detection and Recognition */
static esp_err_t stream_handler(httpd_req_t *req)
{
    camera_fb_t *fb = NULL;
    struct timeval _timestamp;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char part_buf[128];
        bool detected = false;
        int64_t fr_ready = 0;
        int64_t fr_recognize = 0;
        int64_t fr_encode = 0;
        int64_t fr_face = 0;
        int64_t fr_start = 0;
    int face_id = 0;
    size_t out_len = 0, out_width = 0, out_height = 0;
    uint8_t *out_buf = NULL;
    bool s = false;
    // MSR01 params: (score/confidence threshold; nms threshold (IoU); top_K (candidates to return); post filter threshold)
    HumanFaceDetectMSR01 s1(0.2F, 0.1F, 10, 0.2F);
    // MNP01 params: (score/confidence threshold; nms threshold (IoU); top_K (candidates to return))
    HumanFaceDetectMNP01 s2(0.2F, 0.1F, 5);
    static int64_t last_frame = 0;
    if (!last_frame)
    {
        last_frame = esp_timer_get_time();
    }
    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK)
    {
        return res;
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "60");
    // loop to contiuously send frames until disconnect
    while (true)
    {
        detected = false;
        face_id = 0;
        fb = esp_camera_fb_get();
        if (!fb)
        {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
        }
        else
        {
            _timestamp.tv_sec = fb->timestamp.tv_sec;
            _timestamp.tv_usec = fb->timestamp.tv_usec;
            fr_start = esp_timer_get_time();
            fr_ready = fr_start;
            fr_encode = fr_start;
            fr_recognize = fr_start;
            fr_face = fr_start;
            // when not detecting or enrolling, faster camera stream
            if ((!detection_enabled && !is_enrolling) || fb->width > 400)
            {
                if (fb->format != PIXFORMAT_JPEG)
                {
                    bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                    esp_camera_fb_return(fb);
                    fb = NULL;
                    if (!jpeg_converted)
                    {
                        ESP_LOGE(TAG, "JPEG compression failed");
                        res = ESP_FAIL;
                    }
                }
                else
                {
                    _jpg_buf_len = fb->len;
                    _jpg_buf = fb->buf;
                }
            }
            else //if only doing detection, runs on RGB565 path
            {
                if (fb->format == PIXFORMAT_RGB565 && !recognition_enabled && !is_enrolling)
                {
                    fr_ready = esp_timer_get_time();
                    // running detection on the output buffer
                    std::list<dl::detect::result_t> &candidates = s1.infer((uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3});
                    std::list<dl::detect::result_t> &results = s2.infer((uint16_t *)fb->buf, {(int)fb->height, (int)fb->width, 3}, candidates);
                    fr_face = esp_timer_get_time();
                    fr_recognize = fr_face;
                    if (results.size() > 0) {
                        fb_data_t rfb;
                        rfb.width = fb->width;
                        rfb.height = fb->height;
                        rfb.data = fb->buf;
                        rfb.bytes_per_pixel = 2;
                        rfb.format = FB_RGB565;
                        detected = true;
                        draw_face_boxes(&rfb, &results, face_id);
                    }
                    s = fmt2jpg(fb->buf, fb->len, fb->width, fb->height, PIXFORMAT_RGB565, 80, &_jpg_buf, &_jpg_buf_len);
                    esp_camera_fb_return(fb);
                    fb = NULL;
                    if (!s) {
                        ESP_LOGE(TAG, "fmt2jpg failed");
                        res = ESP_FAIL;
                    }
                    fr_encode = esp_timer_get_time();
                } else // full frame stream - needed for facial recognition
                {
                    out_len = fb->width * fb->height * 3;
                    out_width = fb->width;
                    out_height = fb->height;
                    out_buf = (uint8_t*)malloc(out_len);
                    if (!out_buf) {
                        ESP_LOGE(TAG, "out_buf malloc failed");
                        res = ESP_FAIL;
                    } else {
                        s = fmt2rgb888(fb->buf, fb->len, fb->format, out_buf);
                        esp_camera_fb_return(fb);
                        fb = NULL;
                        if (!s) {
                            free(out_buf);
                            ESP_LOGE(TAG, "to rgb888 failed");
                            res = ESP_FAIL;
                        } else {
                            fr_ready = esp_timer_get_time();
                            fb_data_t rfb;
                            rfb.width = out_width;
                            rfb.height = out_height;
                            rfb.data = out_buf;
                            rfb.bytes_per_pixel = 3;
                            rfb.format = FB_BGR888;
                            std::list<dl::detect::result_t> &candidates = s1.infer((uint8_t *)out_buf, {(int)out_height, (int)out_width, 3});
                            std::list<dl::detect::result_t> &results = s2.infer((uint8_t *)out_buf, {(int)out_height, (int)out_width, 3}, candidates);
                            fr_face = esp_timer_get_time();
                            fr_recognize = fr_face;
                            if (results.size() > 0) {
                                detected = true;
                                if (recognition_enabled || is_enrolling) {
                                    face_id = run_face_recognition(&rfb, &results);
                                    fr_recognize = esp_timer_get_time();
                                }
                                draw_face_boxes(&rfb, &results, face_id);
                            }
                            // Keep enrollment message on screen for N ms
                            if (show_enroll_msg) {
                                if (esp_timer_get_time() < enroll_msg_until_us) {
                                    // redisplay enrolled message
                                    rgb_print(&rfb, FACE_COLOR_CYAN, enroll_msg_text);
                                } else {
                                    show_enroll_msg = false; // stop displaying
                                }
                            }
                            s = fmt2jpg(out_buf, out_len, out_width, out_height, PIXFORMAT_RGB888, 90, &_jpg_buf, &_jpg_buf_len);
                            free(out_buf);
                            if (!s) {
                                ESP_LOGE(TAG, "fmt2jpg failed");
                                res = ESP_FAIL;
                            }
                            fr_encode = esp_timer_get_time();
                        }
                    }
                }
            }
        }
        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if (res == ESP_OK)
        {
            size_t hlen = snprintf((char *)part_buf, 128, _STREAM_PART, _jpg_buf_len, _timestamp.tv_sec, _timestamp.tv_usec);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if (res == ESP_OK)
        {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if (fb)
        {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        }
        else if (_jpg_buf)
        {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if (res != ESP_OK)
        {
            ESP_LOGE(TAG, "send frame failed failed");
            break;
        }
        int64_t fr_end = esp_timer_get_time();
        int64_t ready_time = (fr_ready - fr_start) / 1000;
        int64_t face_time = (fr_face - fr_ready) / 1000;
        int64_t recognize_time = (fr_recognize - fr_face) / 1000;
        int64_t encode_time = (fr_encode - fr_recognize) / 1000;
        int64_t process_time = (fr_encode - fr_start) / 1000;
        int64_t frame_time = fr_end - last_frame;
        frame_time /= 1000;
        ESP_LOGI(TAG, "MJPG: %uB %ums (%.1ffps)"
                      ", %u+%u+%u+%u=%u %s%d", (uint32_t)(_jpg_buf_len),
                 (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time,
                 (uint32_t)ready_time, (uint32_t)face_time, (uint32_t)recognize_time, (uint32_t)encode_time, (uint32_t)process_time,
                 (detected) ? "DETECTED " : "", face_id);
    }
    return res;
}


/* helper for parsing query */
static esp_err_t parse_get(httpd_req_t *req, char **obuf)
{
    char *buf = NULL;
    size_t buf_len = 0;
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char *)malloc(buf_len);
        if (!buf) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            *obuf = buf;
            return ESP_OK;
        }
        free(buf);
    }
    httpd_resp_send_404(req);
    return ESP_FAIL;
}


/* handles UI controls */
static esp_err_t cmd_handler(httpd_req_t *req)
{
    char *buf = NULL;
    char variable[32];
    char value[32];
    if (parse_get(req, &buf) != ESP_OK) {
        return ESP_FAIL;
    }
    if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) != ESP_OK) {
        free(buf);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    if (httpd_query_key_value(buf, "val", value, sizeof(value)) != ESP_OK) {
        value[0] = '\0';
    }
    free(buf);
    int val = atoi(value);
    ESP_LOGI(TAG, "%s = %d", variable, val);
    sensor_t *s = esp_camera_sensor_get();
    int res = 0;
    if (!strcmp(variable, "quality"))
        res = s->set_quality(s, val);
    else if (!strcmp(variable, "contrast"))
        res = s->set_contrast(s, val);
    else if (!strcmp(variable, "brightness"))
        res = s->set_brightness(s, val);
    else if (!strcmp(variable, "saturation"))
        res = s->set_saturation(s, val);
    else if (!strcmp(variable, "face_detect")) {
    detection_via_gui = (val != 0);
    if (!detection_via_gui) {
        recognition_via_gui = false;
    }
    recompute_face_state();
    }
    else if (!strcmp(variable, "face_enroll")) {
        if (value != NULL && strlen(value) > 0) {
            int v = atoi(value);
            is_enrolling = (v != 0) ? 1 : 0;
        } else {
            is_enrolling = !is_enrolling;
        }
        recompute_face_state();
    }
    else if (!strcmp(variable, "face_recognize")) {
        recognition_via_gui = (val != 0);
        if (recognition_via_gui) {
            detection_via_gui = true;
        }
        recompute_face_state();
    }
    else {
        ESP_LOGI(TAG, "Unknown command: %s", variable);
        res = -1;
    }
    if (res < 0) {
        return httpd_resp_send_500(req);
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}


/* sends compressed HTTP - UI fully hosted on esp32 - address = ESP IP  */
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        if (s->id.PID == OV2640_PID){
            return httpd_resp_send(req, (const char *)index_ov2640_html_gz, index_ov2640_html_gz_len);
        }
    } else {
        ESP_LOGE(TAG, "Camera sensor not found");
        return httpd_resp_send_500(req);
    }
}


/* main setup for app and its endpoints  */
void startCameraServer()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = NULL
    };
    httpd_uri_t cmd_uri = {
        .uri = "/control",
        .method = HTTP_GET,
        .handler = cmd_handler,
        .user_ctx = NULL
    };
    httpd_uri_t stream_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = NULL
    };
    // face recognizer
    recognizer.set_partition(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "fr");
    recognizer.set_ids_from_flash();     // load ids from flash partition
    ESP_LOGI(TAG, "Starting web server on port: '%d'", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK)
    {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &cmd_uri);
    }
    config.server_port += 1;
    config.ctrl_port += 1;
    ESP_LOGI(TAG, "Starting stream server on port: '%d'", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK)
    {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}



