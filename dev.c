/*
 * Sigma Control API DUT (station/AP/sniffer)
 * Copyright (c) 2011-2013, 2017, Qualcomm Atheros, Inc.
 * Copyright (c) 2018-2019, The Linux Foundation
 * All Rights Reserved.
 * Licensed under the Clear BSD license. See README for more details.
 */

#include "sigma_dut.h"
#include <ctype.h>
#include "miracast.h"
#include <sys/wait.h>
#include "wpa_ctrl.h"
#include "wpa_helpers.h"


static enum sigma_cmd_result cmd_dev_send_frame(struct sigma_dut *dut,
						struct sigma_conn *conn,
						struct sigma_cmd *cmd)
{
#ifdef MIRACAST
	const char *program = get_param(cmd, "Program");

	if (program && (strcasecmp(program, "WFD") == 0 ||
			strcasecmp(program, "DisplayR2") == 0))
		return miracast_dev_send_frame(dut, conn, cmd);
#endif /* MIRACAST */

	if (dut->mode == SIGMA_MODE_STATION ||
	    dut->mode == SIGMA_MODE_UNKNOWN) {
		sigma_dut_print(dut, DUT_MSG_DEBUG, "Convert "
				"dev_send_frame to sta_send_frame");
		return cmd_sta_send_frame(dut, conn, cmd);
	}

	if (dut->mode == SIGMA_MODE_AP) {
		sigma_dut_print(dut, DUT_MSG_DEBUG, "Convert "
				"dev_send_frame to ap_send_frame");
		return cmd_ap_send_frame(dut, conn, cmd);
	}

#ifdef CONFIG_WLANTEST
	sigma_dut_print(dut, DUT_MSG_DEBUG, "Convert dev_send_frame to "
			"wlantest_send_frame");
	return cmd_wlantest_send_frame(dut, conn, cmd);
#else /* CONFIG_WLANTEST */
	send_resp(dut, conn, SIGMA_ERROR,
		  "errorCode,Unsupported dev_send_frame");
	return STATUS_SENT;
#endif /* CONFIG_WLANTEST */
}


static enum sigma_cmd_result cmd_dev_set_parameter(struct sigma_dut *dut,
						   struct sigma_conn *conn,
						   struct sigma_cmd *cmd)
{
	const char *device = get_param(cmd, "Device");

	if (device && strcasecmp(device, "STA") == 0) {
		sigma_dut_print(dut, DUT_MSG_DEBUG, "Convert "
				"dev_set_parameter to sta_set_parameter");
		return cmd_sta_set_parameter(dut, conn, cmd);
	}

	return INVALID_SEND_STATUS;
}


static enum sigma_cmd_result cmd_dev_exec_action(struct sigma_dut *dut,
						 struct sigma_conn *conn,
						 struct sigma_cmd *cmd)
{
	const char *program = get_param(cmd, "Program");

#ifdef MIRACAST
	if (program && (strcasecmp(program, "WFD") == 0 ||
			strcasecmp(program, "DisplayR2") == 0)) {
		if (get_param(cmd, "interface") == NULL)
			return INVALID_SEND_STATUS;
		return miracast_dev_exec_action(dut, conn, cmd);
	}
#endif /* MIRACAST */

	if (program && strcasecmp(program, "DPP") == 0)
		return dpp_dev_exec_action(dut, conn, cmd);

	return ERROR_SEND_STATUS;
}


static enum sigma_cmd_result cmd_dev_configure_ie(struct sigma_dut *dut,
						  struct sigma_conn *conn,
						  struct sigma_cmd *cmd)
{
	const char *ie_name = get_param(cmd, "IE_Name");
	const char *contents = get_param(cmd, "Contents");

	if (!ie_name || !contents)
		return INVALID_SEND_STATUS;

	if (strcasecmp(ie_name, "RSNE") != 0) {
		send_resp(dut, conn, SIGMA_ERROR,
			  "errorCode,Unsupported IE_Name value");
		return STATUS_SENT;
	}

	free(dut->rsne_override);
	dut->rsne_override = strdup(contents);

	return dut->rsne_override ? SUCCESS_SEND_STATUS : ERROR_SEND_STATUS;
}


static enum sigma_cmd_result cmd_dev_ble_action(struct sigma_dut *dut,
						struct sigma_conn *conn,
						struct sigma_cmd *cmd)
{
#ifdef ANDROID
	const char *ble_op = get_param(cmd, "BLEOp");
	const char *prog = get_param(cmd, "Prog");
	const char *service_name = get_param(cmd, "ServiceName");
	const char *ble_role = get_param(cmd, "BLERole");
	const char *discovery_type = get_param(cmd, "DiscoveryType");
	const char *msg_type = get_param(cmd, "messagetype");
	const char *action = get_param(cmd, "action");
	const char *M2Transmit = get_param(cmd, "M2Transmit");
	char *argv[17];
	pid_t pid;

	if (prog && ble_role && action && msg_type) {
		send_resp(dut, conn, SIGMA_COMPLETE,
			  "OrgID,0x00,TransDataHeader,0x00,BloomFilterElement,NULL");
		return STATUS_SENT;
	}
	if (!ble_op || !prog || !service_name || !ble_role || !discovery_type) {
		sigma_dut_print(dut, DUT_MSG_ERROR, "Invalid arguments");
		return INVALID_SEND_STATUS;
	}

	if ((strcasecmp(prog, "NAN") != 0)) {
		sigma_dut_print(dut, DUT_MSG_ERROR, "Program %s not supported",
				prog);
		return INVALID_SEND_STATUS;
	}

	if (strcasecmp(ble_role, "seeker") != 0 &&
	    strcasecmp(ble_role, "provider") != 0 &&
	    strcasecmp(ble_role, "browser") != 0) {
		sigma_dut_print(dut, DUT_MSG_ERROR, "Invalid BLERole: %s",
				ble_role);
		return INVALID_SEND_STATUS;
	}

	if (strcasecmp(discovery_type, "active") != 0 &&
	    strcasecmp(discovery_type, "passive") != 0) {
		sigma_dut_print(dut, DUT_MSG_ERROR, "Invalid DiscoveryType: %s",
				discovery_type);
		return INVALID_SEND_STATUS;
	}

	if (!M2Transmit)
		M2Transmit = "disable";

	argv[0] = "am";
	argv[1] = "start";
	argv[2] = "-n";
	argv[3] = "org.codeaurora.nanservicediscovery/org.codeaurora.nanservicediscovery.MainActivity";
	argv[4] = "--es";
	argv[5] = "service";
	argv[6] = (char *) service_name;
	argv[7] = "--es";
	argv[8] = "role";
	argv[9] = (char *) ble_role;
	argv[10] = "--es";
	argv[11] = "scantype";
	argv[12] = (char *) discovery_type;
	argv[13] = "--es";
	argv[14] = "M2Transmit";
	argv[15] = (char *) M2Transmit;
	argv[16] = NULL;

	pid = fork();
	if (pid == -1) {
		sigma_dut_print(dut, DUT_MSG_ERROR, "fork: %s",
				strerror(errno));
		return ERROR_SEND_STATUS;
	}

	if (pid == 0) {
		execv("/system/bin/am", argv);
		sigma_dut_print(dut, DUT_MSG_ERROR, "execv: %s",
				strerror(errno));
		exit(0);
		return ERROR_SEND_STATUS;
	}

	dut->nanservicediscoveryinprogress = 1;
#endif /* ANDROID */

	return SUCCESS_SEND_STATUS;
}


/* Runtime ID must contain only numbers */
static int is_runtime_id_valid(struct sigma_dut *dut, const char *val)
{
	int i;

	for (i = 0; val[i] != '\0'; i++) {
		if (!isdigit(val[i])) {
			sigma_dut_print(dut, DUT_MSG_DEBUG,
					"Invalid Runtime_ID %s", val);
			return 0;
		}
	}

	return 1;
}


static int build_log_dir(struct sigma_dut *dut, char *dir, size_t dir_size)
{
	int res;
	const char *vendor;
	int i;

	vendor = dut->vendor_name ? dut->vendor_name : "Qualcomm";

	if (dut->log_file_dir) {
		res = snprintf(dir, dir_size, "%s/%s", dut->log_file_dir,
			       vendor);
	} else {
#ifdef ANDROID
		res = snprintf(dir, dir_size, "/data/vendor/wifi/%s",
			       vendor);
#else /* ANDROID */
		res = snprintf(dir, dir_size, "/var/log/%s", vendor);
#endif /* ANDROID */
	}

	if (res < 0 || res >= dir_size)
		return -1;

	/* Check for valid vendor name in log dir path since the log dir
	 * (/var/log/vendor) is deleted in dev_stop routine. This check is to
	 * avoid any unintended file deletion.
	 */
	for (i = 0; vendor[i] != '\0'; i++) {
		if (!isalpha(vendor[i])) {
			sigma_dut_print(dut, DUT_MSG_DEBUG,
					"Invalid char %c in vendor name %s",
					vendor[i], vendor);
			return -1;
		}
	}

	return 0;
}


/* User has to redirect wpa_supplicant logs to the following file. */
#ifndef WPA_SUPPLICANT_LOG_FILE
#define WPA_SUPPLICANT_LOG_FILE "/var/log/supplicant_log/wpa_log.txt"
#endif /* WPA_SUPPLICANT_LOG_FILE */

static enum sigma_cmd_result cmd_dev_start_test(struct sigma_dut *dut,
						struct sigma_conn *conn,
						struct sigma_cmd *cmd)
{
	const char *val;
	char buf[250];
	char dir[200];
	FILE *supp_log;
	int res;

	val = get_param(cmd, "Runtime_ID");
	if (!(val && is_runtime_id_valid(dut, val)))
		return INVALID_SEND_STATUS;

	if (build_log_dir(dut, dir, sizeof(dir)) < 0)
		return ERROR_SEND_STATUS;

	supp_log = fopen(WPA_SUPPLICANT_LOG_FILE, "r");
	if (!supp_log) {
		sigma_dut_print(dut, DUT_MSG_ERROR,
				"Failed to open wpa_log file %s",
				WPA_SUPPLICANT_LOG_FILE);
	} else {
		/* Get the wpa_supplicant log file size */
		if (fseek(supp_log, 0, SEEK_END))
			sigma_dut_print(dut, DUT_MSG_ERROR,
					"Failed to get file size for read");
		else
			dut->wpa_log_size = ftell(supp_log);

		fclose(supp_log);
	}

	strlcpy(dut->dev_start_test_runtime_id, val,
		sizeof(dut->dev_start_test_runtime_id));
	sigma_dut_print(dut, DUT_MSG_DEBUG, "Runtime_ID %s",
			dut->dev_start_test_runtime_id);

	run_system_wrapper(dut, "rm -rf %s", dir);
	run_system_wrapper(dut, "mkdir -p %s", dir);

#ifdef ANDROID
	run_system_wrapper(dut, "logcat -v time > %s/logcat_%s.txt &",
			   dir, dut->dev_start_test_runtime_id);
#else /* ANDROID */
	/* Open log file for sigma_dut logs. This is not needed for Android, as
	 * we are already collecting logcat. */
	res = snprintf(buf, sizeof(buf), "%s/sigma_%s.txt", dir,
		       dut->dev_start_test_runtime_id);
	if (res >= 0 && res < sizeof(buf)) {
		if (dut->log_file_fd)
			fclose(dut->log_file_fd);

		dut->log_file_fd = fopen(buf, "a");
		if (!dut->log_file_fd)
			sigma_dut_print(dut, DUT_MSG_ERROR,
					"Failed to create sigma_dut log %s",
					buf);
	}

	run_system_wrapper(dut, "killall -9 cnss_diag_lite");
	run_system_wrapper(dut,
			   "cnss_diag_lite -c -x 31 > %s/cnss_diag_id_%s.txt &",
			   dir, dut->dev_start_test_runtime_id);
#endif /* ANDROID */

	return SUCCESS_SEND_STATUS;
}


static int is_allowed_char(char ch)
{
	return strchr("./-_", ch) != NULL;
}


static int is_destpath_valid(struct sigma_dut *dut, const char *val)
{
	int i;

	for (i = 0; val[i] != '\0'; i++) {
		if (!(isalnum(val[i]) || is_allowed_char(val[i]))) {
			sigma_dut_print(dut, DUT_MSG_DEBUG,
					"Invalid char %c in destpath %s",
					val[i], val);
			return 0;
		}
	}

	return 1;
}


#ifndef ANDROID
#define SUPP_LOG_BUFF_SIZE 4 * 1024

static int save_supplicant_log(struct sigma_dut *dut)
{
	char dir[200];
	char buf[300];
	FILE *wpa_log = NULL;
	FILE *supp_log;
	char *buff_ptr = NULL;
	unsigned int file_size;
	unsigned int file_size_orig;
	int status = -1, res;

	if (build_log_dir(dut, dir, sizeof(dir)) < 0)
		return -1;

	res = snprintf(buf, sizeof(buf), "%s/wpa_supplicant_log_%s.txt", dir,
		       dut->dev_start_test_runtime_id);
	if (res < 0 || res >= sizeof(buf))
		return -1;

	supp_log = fopen(WPA_SUPPLICANT_LOG_FILE, "r");
	if (!supp_log) {
		sigma_dut_print(dut, DUT_MSG_ERROR,
				"Failed to open wpa_log file %s",
				WPA_SUPPLICANT_LOG_FILE);
		return -1;
	}

	/* Get the wpa_supplicant log file size */
	if (fseek(supp_log, 0, SEEK_END)) {
		sigma_dut_print(dut, DUT_MSG_ERROR,
				"Failed to get file size for read");
		goto exit;
	}
	file_size_orig = ftell(supp_log);

	if (file_size_orig < dut->wpa_log_size) {
		sigma_dut_print(dut, DUT_MSG_ERROR,
				"file size err, new size %u, old size %u",
				file_size_orig, dut->wpa_log_size);
		goto exit;
	}

	/* Get the wpa_supplicant file size for current test */
	file_size = file_size_orig - dut->wpa_log_size;

	wpa_log = fopen(buf, "w");
	if (!wpa_log) {
		sigma_dut_print(dut, DUT_MSG_ERROR,
				"Failed to create tmp wpa_log file %s", buf);
		goto exit;
	}

	if (fseek(supp_log, dut->wpa_log_size, SEEK_SET)) {
		sigma_dut_print(dut, DUT_MSG_ERROR,
				"Failed to set wpa_log file ptr for read");
		goto exit;
	}

	buff_ptr = malloc(SUPP_LOG_BUFF_SIZE);
	if (!buff_ptr) {
		sigma_dut_print(dut, DUT_MSG_ERROR,
				"Failed to alloc buffer of size %d",
				SUPP_LOG_BUFF_SIZE);
		goto exit;
	}

	/* Read wpa_supplicant log file in 4K byte chunks */
	do {
		unsigned int num_bytes_to_read;
		unsigned int bytes_read;

		num_bytes_to_read = (file_size > SUPP_LOG_BUFF_SIZE) ?
			SUPP_LOG_BUFF_SIZE : file_size;
		bytes_read = fread(buff_ptr, 1, num_bytes_to_read, supp_log);
		if (!bytes_read) {
			sigma_dut_print(dut, DUT_MSG_ERROR,
					"Failed to read wpa_supplicant log");
			goto exit;
		}
		if (bytes_read != num_bytes_to_read) {
			sigma_dut_print(dut, DUT_MSG_DEBUG,
					"wpa_supplicant log read err, read %d, num_bytes_to_read %d",
					bytes_read, num_bytes_to_read);
			goto exit;
		}
		fwrite(buff_ptr, 1, bytes_read, wpa_log);
		file_size -= bytes_read;
	} while (file_size > 0);
	status = 0;

exit:
	if (wpa_log)
		fclose(wpa_log);
	fclose(supp_log);
	free(buff_ptr);

	return status;
}
#endif /* !ANDROID */


static enum sigma_cmd_result cmd_dev_stop_test(struct sigma_dut *dut,
					       struct sigma_conn *conn,
					       struct sigma_cmd *cmd)
{
	const char *val;
	char buf[300];
	char out_file[100];
	char dir[200];
	int res;

	val = get_param(cmd, "Runtime_ID");
	if (!val || strcmp(val, dut->dev_start_test_runtime_id) != 0) {
		sigma_dut_print(dut, DUT_MSG_ERROR, "Invalid runtime id");
		return ERROR_SEND_STATUS;
	}

	if (build_log_dir(dut, dir, sizeof(dir)) < 0)
		return ERROR_SEND_STATUS;

#ifdef ANDROID
	/* Copy all cnss_diag logs to dir */
	run_system_wrapper(dut, "cp -a /data/vendor/wifi/wlan_logs/* %s", dir);
#else /* ANDROID */
	if (dut->log_file_fd) {
		fclose(dut->log_file_fd);
		dut->log_file_fd = NULL;
	}
	if (save_supplicant_log(dut))
		sigma_dut_print(dut, DUT_MSG_ERROR,
				"Failed to save wpa_supplicant log");
#endif /* ANDROID */

	res = snprintf(out_file, sizeof(out_file), "%s_%s_%s.tar.gz",
		       dut->vendor_name ? dut->vendor_name : "Qualcomm",
		       dut->model_name ? dut->model_name : "Unknown",
		       dut->dev_start_test_runtime_id);
	if (res < 0 || res >= sizeof(out_file))
	    return ERROR_SEND_STATUS;

	if (run_system_wrapper(dut, "tar -czvf %s/../%s %s", dir, out_file,
			       dir) < 0) {
		sigma_dut_print(dut, DUT_MSG_ERROR, "Failed to create tar: %s",
				buf);
		return ERROR_SEND_STATUS;
	}

	val = get_param(cmd, "destpath");
	if (!(val && is_destpath_valid(dut, val))) {
		sigma_dut_print(dut, DUT_MSG_DEBUG, "Invalid path for TFTP %s",
				val);
		return ERROR_SEND_STATUS;
	}

	res = snprintf(buf, sizeof(buf), "tftp %s -c put %s/%s %s/%s",
		       inet_ntoa(conn->addr.sin_addr), dir, out_file, val,
		       out_file);
	if (res < 0 || res >= sizeof(buf))
		return ERROR_SEND_STATUS;
	if (run_system_wrapper(dut, buf) < 0) {
		sigma_dut_print(dut, DUT_MSG_ERROR,
				"TFTP file transfer failed: %s", buf);
		return ERROR_SEND_STATUS;
	}
	sigma_dut_print(dut, DUT_MSG_DEBUG, "TFTP file transfer: %s", buf);
	snprintf(buf, sizeof(buf), "filename,%s", out_file);
	send_resp(dut, conn, SIGMA_COMPLETE, buf);
	run_system_wrapper(dut, "rm -f %s/../%s", dir, out_file);
	run_system_wrapper(dut, "rm -rf %s", dir);

	return SUCCESS_SEND_STATUS;
}


static enum sigma_cmd_result cmd_dev_get_log(struct sigma_dut *dut,
					     struct sigma_conn *conn,
					     struct sigma_cmd *cmd)
{
	return SUCCESS_SEND_STATUS;
}


static int req_intf(struct sigma_cmd *cmd)
{
	return get_param(cmd, "interface") == NULL ? -1 : 0;
}


static int req_role_svcname(struct sigma_cmd *cmd)
{
	if (!get_param(cmd, "BLERole"))
		 return -1;
	if (get_param(cmd, "BLEOp") && !get_param(cmd, "ServiceName"))
		return -1;
	return 0;
}


static int req_intf_prog(struct sigma_cmd *cmd)
{
	if (get_param(cmd, "interface") == NULL)
		return -1;
	if (get_param(cmd, "program") == NULL)
		return -1;
	return 0;
}


static int req_prog(struct sigma_cmd *cmd)
{
	if (get_param(cmd, "program") == NULL)
		return -1;
	return 0;
}


void dev_register_cmds(void)
{
	sigma_dut_reg_cmd("dev_send_frame", req_prog, cmd_dev_send_frame);
	sigma_dut_reg_cmd("dev_set_parameter", req_intf_prog,
			  cmd_dev_set_parameter);
	sigma_dut_reg_cmd("dev_exec_action", req_prog,
			  cmd_dev_exec_action);
	sigma_dut_reg_cmd("dev_configure_ie", req_intf, cmd_dev_configure_ie);
	sigma_dut_reg_cmd("dev_start_test", NULL, cmd_dev_start_test);
	sigma_dut_reg_cmd("dev_stop_test", NULL, cmd_dev_stop_test);
	sigma_dut_reg_cmd("dev_get_log", NULL, cmd_dev_get_log);
	sigma_dut_reg_cmd("dev_ble_action", req_role_svcname,
			  cmd_dev_ble_action);
}
