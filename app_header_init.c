#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <regex.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <signal.h>
#include <sys/msg.h>
#include <pthread.h>
#include <assert.h>
#include <regex.h>


#include <MemBroker/mem_broker.h>
#include "kp_struct.h"
#include "model_type.h"
#include "kdp2_inf_app_yolo.h"
#include "demo_customize_inf_single_model.h"
#include "kdp2_host_stream.h"

//#define ENABLE_DBG_LOG

#ifdef ENABLE_DBG_LOG
#define dbg_log(__format__, ...) printf(""__format__, ##__VA_ARGS__)
#else
#define dbg_log(__format__, ...)
#endif

#define FDFR_MODEL_SIZE_ROW         90
#define FDFR_MODEL_SIZE_COL         90
#define MAX_RESULT_BOX              80

static bool _init_config_yolo_params = false;
extern unsigned int g_dwDrawBoxEnable;
extern unsigned int g_dwDrawBoxType;

static kp_app_yolo_post_proc_config_t post_proc_params_v5s = {
    .prob_thresh = 0.15,
    .nms_thresh = 0.5,
    .max_detection_per_class = 100,
    .anchor_row = 3,
    .anchor_col = 6,
    .stride_size = 3,
    .reserved_size = 0,
    .data = {
        // anchors[3][6]
        10, 13, 16, 30, 33, 23,
        30, 61, 62, 45, 59, 119,
        116, 90, 156, 198, 373, 326,
        // strides[3]
        8, 16, 32,
    },
};

static kp_app_yolo_post_proc_config_t post_proc_params_v5_vd = {
    .prob_thresh = 0.15,
    .nms_thresh = 0.5,
    .max_detection_per_class = 100,
    .anchor_row = 3,
    .anchor_col = 6,
    .stride_size = 3,
    .reserved_size = 0,
    .data = {
        // anchors[3][6]
        7, 8, 16, 12, 11, 26,
        27, 20, 48, 36, 28, 69,
        95, 62, 70, 172, 184, 242,
        // strides[3]
        8, 16, 32,
    },
};


static pid_t pid = -1;
static int once = 0;

static char *stripCrLf(char *line) {
    int len = 0;

    if(line == 0)		return 0;
    len = strlen(line);
    if(len > 1000)		return line;
    if(len <= 0)		return line;
    if (line[len - 1] == '\n' || line[len - 1] == '\r')
        line[len - 1] = 0;
    len = strlen(line);
    if (len > 0)
        if (line[len - 1] == '\n' || line[len - 1] == '\r')
            line[len - 1] = 0;
    return line;
}
static int pidof(const char *name, pid_t *process) {
    regex_t Regx;
    char Pattern[128] = {0}, cmd[128] = {0}, line[128] = {0}, num[64] = {0};
    FILE *f = 0;
    regmatch_t Match[64];
    int len = 0, id = -1, I = 0;

    strcpy(Pattern, "^([0-9]{1,})$"), regcomp(&Regx, Pattern, REG_EXTENDED);
    sprintf(cmd, "pidof %s", name);
    f = popen(cmd, "r");
    if (0 == f)        return -1;
    fgets(line, sizeof(line), f);
    stripCrLf(line);
    if (0 != regexec(&Regx, line, sizeof(Match) / sizeof(regmatch_t), Match, 0)) {
        regfree(&Regx), pclose(f), f = 0;
        return -1;
    }
    I = 1, len = Match[I].rm_eo - Match[I].rm_so, memcpy(num, line + Match[I].rm_so, len), num[len] = 0;
    sscanf(num, "%d", &id);
    if (process)    *process = id;
    regfree(&Regx), pclose(f), f = 0;
    return 0;
}


static void print_yolo_result(kp_app_yolo_result_t *yolo_data)
{
    unsigned int i = 0;
    static unsigned int dwResultCounts = 0;
    int n = 0;

    if (0 == once) {
        once = 1;
        pidof("sigw", &pid);
    }

    if (dwResultCounts > MAX_RESULT_BOX)
        dwResultCounts = MAX_RESULT_BOX;

    if (g_dwDrawBoxEnable) {
        for (i = 0 ; i < dwResultCounts; i++) {
            if (g_atDrawInfo[i].bDrawFlag && !g_dwDrawBoxType)
                setup_isp_privacy_mask(0, g_atDrawInfo[i].dwStartX, g_atDrawInfo[i].dwStartY, g_atDrawInfo[i].dwWidth, g_atDrawInfo[i].dwHeight);
        }
        memset(&g_atDrawInfo, 0, sizeof(g_atDrawInfo));
        dwResultCounts = yolo_data->box_count;

        g_dwResultCounts = 0;
        for (i = 0; i < yolo_data->box_count; i++) {
            if (g_dwOnlyPerson) {
                if (yolo_data->boxes[i].class_num != 0)
                    continue;
            }
            calculate_bbox_postion(&g_atDrawInfo[i], yolo_data->boxes[i].x1, yolo_data->boxes[i].y1,
                yolo_data->boxes[i].x2, yolo_data->boxes[i].y2, yolo_data->boxes[i].score, yolo_data->boxes[i].class_num);
            g_atDrawInfo[i].bDrawFlag = true;
            if(!g_dwDrawBoxType) {
                setup_isp_privacy_mask(1, g_atDrawInfo[i].dwStartX, g_atDrawInfo[i].dwStartY, g_atDrawInfo[i].dwWidth, g_atDrawInfo[i].dwHeight);
            }
            g_dwResultCounts++;
        }
        if(g_dwResultCounts > MAX_RESULT_BOX) { g_dwResultCounts = MAX_RESULT_BOX; }
    }

    n = 0;
    for (i = 0; i < yolo_data->box_count; i++) {
        if (g_dwOnlyPerson) {
            if (yolo_data->boxes[i].class_num != 0)
                continue;
        }

        printf("[%s][%d] [AI coordinate] Count = %d x1 = %f y1 = %f x2 = %f y2 = %f score = %f class_num = %d\n", __func__, i,
            yolo_data->box_count, yolo_data->boxes[i].x1, yolo_data->boxes[i].y1,
            yolo_data->boxes[i].x2, yolo_data->boxes[i].y2, yolo_data->boxes[i].score, yolo_data->boxes[i].class_num);
        n++;
    }

    if (n > 0 && -1 != pid) {
        int r = 0;
        char path[1024] = {0};
        FILE *f = 0;
        union sigval sigVal = {0};
        char yaml[1024 * 8] = {0};
        char tmp[1024] = {0};
        char *p;

        while (0 == r)
            srand(time(NULL)), r = rand();
        sprintf(path, "/tmp/usr-%08X.yaml", r);
        f = fopen(path, "wb"), assert(0 != f);

	strcat(yaml, "---\n");
	strcat(yaml, "face:\n");
        for (i = 0; i < yolo_data->box_count; i++) {
            if (g_dwOnlyPerson) {
                if (yolo_data->boxes[i].class_num != 0)
                    continue;
            }
            sprintf(tmp, "  %d:\n", i), strcat(yaml, tmp);
            sprintf(tmp, "    id: %d\n", 80 + i), strcat(yaml, tmp);
            sprintf(tmp, "    box: %f %f %f %f\n", yolo_data->boxes[i].x1, yolo_data->boxes[i].y1, yolo_data->boxes[i].x2, yolo_data->boxes[i].y2), strcat(yaml, tmp);
            sprintf(tmp, "    score: %f\n", yolo_data->boxes[i].score), strcat(yaml, tmp);
            sprintf(tmp, "    kps: 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00 0.00\n"), strcat(yaml, tmp);
        }
	sprintf(tmp, "\n\n"), strcat(yaml, tmp);

        fwrite(yaml, 1, strlen(yaml), f);
        fclose(f), f = 0;

        memset(&sigVal, 0, sizeof(union sigval));
        sigVal.sival_int = r;

        if(sigqueue(pid, SIGUSR1, sigVal) == -1) {
	    unlink(path);
        }
    }

}

/****************************************************************
 * application header/result callback function (Please override the callback functions for other application verify)
 ****************************************************************/
int app_header_send_inference(uint32_t buf_addr, bool *bl_run_next_inference, void* arg, unsigned char* image_buffer, VMF_VSRC_SSM_OUTPUT_INFO_T* vsrc_ssm_info)
{
    HOST_STREAM_INIT_OPT_T* pInitOpt=(HOST_STREAM_INIT_OPT_T*)arg;
    unsigned int dwWidth = pInitOpt->tVEncoder[pInitOpt->dwInferenceStream].dwWidth;
    unsigned int dwHeight = pInitOpt->tVEncoder[pInitOpt->dwInferenceStream].dwHeight;
    int ret = 0;

    kp_image_format_t kp_image_format = KP_IMAGE_FORMAT_YUV420; //The default image format of host_stream/hico_mipi is KP_IMAGE_FORMAT_YUV420.

    if(pInitOpt->dwJobId == KDP2_INF_ID_APP_YOLO)
    {
        if (false == _init_config_yolo_params) {
            /****************************************************************
             * configure application pre/post-processing params
             ****************************************************************/
            _init_config_yolo_params = true;

            kdp2_ipc_app_yolo_post_proc_config_t *app_yolo_post_proc_config = (kdp2_ipc_app_yolo_post_proc_config_t *)buf_addr;
            kp_inference_header_stamp_t *header_stamp = &app_yolo_post_proc_config->header_stamp;

            header_stamp->magic_type = KDP2_MAGIC_TYPE_INFERENCE;
            header_stamp->total_size = sizeof(kdp2_ipc_app_yolo_post_proc_config_t);
            header_stamp->total_image = 1;
            header_stamp->image_index = 1;
            header_stamp->job_id = KDP2_JOB_ID_APP_YOLO_CONFIG_POST_PROC;

            app_yolo_post_proc_config->model_id = pInitOpt->dwModelId;
            app_yolo_post_proc_config->param_size = sizeof(kp_app_yolo_post_proc_config_t);
            app_yolo_post_proc_config->set_or_get = 1;


            switch (pInitOpt->dwModelId)
            {
            case KNERON_YOLOV5S_COCO80_640_640_3:
            case KNERON_YOLOV5S_PersonBottleChairPottedplant4_288_640_3:
            case KNERON_YOLOV5S_PersonBicycleCarMotorcycleBusTruckCatDog8_256_480_3:
                post_proc_params_v5s.prob_thresh = pInitOpt->fThreshold;
                memcpy((void *)app_yolo_post_proc_config->param_data, (void *)&post_proc_params_v5s, sizeof(kp_app_yolo_post_proc_config_t));
                break;
            case KNERON_YOLOV5S0375_PersonBicycleCarMotorcycleBusTruck6_256_352_3:
                post_proc_params_v5_vd.prob_thresh = pInitOpt->fThreshold;
                memcpy((void *)app_yolo_post_proc_config->param_data, (void *)&post_proc_params_v5_vd, sizeof(kp_app_yolo_post_proc_config_t));
                break;
            default:
                // cannot find matched post-proc config
                printf("%s, model_id = %d, cannot find matched post-proc config\n", __func__, pInitOpt->dwModelId);
                break;
            }

            return KP_SUCCESS;
        }

        kdp2_ipc_app_yolo_inf_header_t *app_yolo_header = (kdp2_ipc_app_yolo_inf_header_t *)buf_addr;
        kp_inference_header_stamp_t *header_stamp = &app_yolo_header->header_stamp;
        static int inf_number = 0;
        inf_number = inf_number+1;
        inf_number &= 65535;

        header_stamp->magic_type = KDP2_MAGIC_TYPE_INFERENCE;
        header_stamp->total_size = sizeof(kdp2_ipc_app_yolo_inf_header_t) + (uint32_t)(vsrc_ssm_info->dwWidth*vsrc_ssm_info->dwHeight*1.5);
        header_stamp->total_image = 1;
        header_stamp->image_index = 1;
        header_stamp->job_id = KDP2_INF_ID_APP_YOLO;

        app_yolo_header->inf_number = inf_number;
        app_yolo_header->model_id = pInitOpt->dwModelId;
        app_yolo_header->width = dwWidth;
        app_yolo_header->height = dwHeight;

        app_yolo_header->image_format = kp_image_format;
        app_yolo_header->model_normalize = KP_NORMALIZE_KNERON;//KP_NORMALIZE_DISABLE;
        printf("[SEND] inf_number = %d \n", inf_number);
        //memcpy((void *)(buf_addr + sizeof(kdp2_ipc_app_yolo_inf_header_t)), image_buffer, image_buffer_size);
        ret = dma2d_copy(pInitOpt->ptDmaInfo->ptDmaHandle, pInitOpt->ptDmaInfo->ptDmaDesc, (void *)(buf_addr + sizeof(kdp2_ipc_app_yolo_inf_header_t)), (void*)image_buffer, vsrc_ssm_info);
        if(ret){
            printf("dma2d_copy failed\n");
        }

        //_inference_number++;

        /****************************************************************
         * finish application - close send thead
         ****************************************************************/
        //if (_loop_time <= _inference_number)
        //    *bl_run_next_inference = false;
    }
    else if(pInitOpt->dwJobId == DEMO_KL630_CUSTOMIZE_INF_SINGLE_MODEL_JOB_ID)
    {
        demo_customize_inf_single_model_header_t *pAppSingle = (demo_customize_inf_single_model_header_t *)buf_addr;
        kp_inference_header_stamp_t *header_stamp = &pAppSingle->header_stamp;

        header_stamp->magic_type = KDP2_MAGIC_TYPE_INFERENCE;
        header_stamp->total_size = sizeof(demo_customize_inf_single_model_header_t) + (uint32_t)(vsrc_ssm_info->dwWidth*vsrc_ssm_info->dwHeight*1.5);
        header_stamp->total_image = 1;
        header_stamp->image_index = 0;
        header_stamp->job_id = DEMO_KL630_CUSTOMIZE_INF_SINGLE_MODEL_JOB_ID;

        pAppSingle->width = dwWidth;
        pAppSingle->height = dwHeight;

        //memcpy((void *)(buf_addr + sizeof(demo_customize_inf_single_model_header_t)), image_buffer, image_buffer_size);
        ret = dma2d_copy(pInitOpt->ptDmaInfo->ptDmaHandle, pInitOpt->ptDmaInfo->ptDmaDesc, (void *)(buf_addr + sizeof(demo_customize_inf_single_model_header_t)), (void*)image_buffer, vsrc_ssm_info);
        if(ret){
            printf("dma2d_copy failed\n");
        }
    }
    else
    {
        printf("[%s] Error: Job ID %u\n", __FUNCTION__, pInitOpt->dwJobId);
        return KP_FW_ERROR_UNKNOWN_APP;
    }

    return KP_SUCCESS;
}

int app_header_recv_inference(uint32_t buf_addr, bool *bl_run_next_inference)
{
    kp_inference_header_stamp_t *header_stamp = (kp_inference_header_stamp_t *)buf_addr;

    if (KDP2_INF_ID_APP_YOLO == header_stamp->job_id) {
        /****************************************************************
         * receive & dump application result
         ****************************************************************/
        if (KP_SUCCESS != header_stamp->status_code) {
            printf("[%s] Error: Run application fail %u\n", __func__, header_stamp->status_code);
            return header_stamp->status_code;
        }

        kdp2_ipc_app_yolo_result_t *app_yolo_result = (kdp2_ipc_app_yolo_result_t *)header_stamp;
        kp_app_yolo_result_t *yolo_data = &app_yolo_result->yolo_data;
        printf("[RECV] inf_number = %d \n", app_yolo_result->inf_number);
        print_yolo_result(yolo_data);
        /****************************************************************
         * finish application - close receive thead
         ****************************************************************/
        //if ((_loop_time - 1) <= app_yolo_result->inf_number)
        //    *bl_run_next_inference = false;
    }
    else if (KDP2_JOB_ID_APP_YOLO_CONFIG_POST_PROC == header_stamp->job_id) {
        /****************************************************************
         * receive pre/post-processing configuration respones
         ****************************************************************/
        if (KP_SUCCESS != header_stamp->status_code) {
            printf("[%s] Error: config post-processing params fail %u\n", __func__, header_stamp->status_code);
            return header_stamp->status_code;
        }

        printf("[%s] config post-processing params success.\n", __func__);
    }
    else if(DEMO_KL630_CUSTOMIZE_INF_SINGLE_MODEL_JOB_ID == header_stamp->job_id)
    {
        printf("[%s] Job ID %u\n", __FUNCTION__, header_stamp->job_id);
    }
    else {
        printf("[%s] Error: Job ID %u\n", __FUNCTION__, header_stamp->job_id);
        return KP_FW_ERROR_UNKNOWN_APP;
    }

    return KP_SUCCESS;
}

