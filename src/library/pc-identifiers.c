/*
 * pc-identifiers.c
 *
 *  Created on: Apr 16, 2014
 *      Author: devel
 */

#include "os/os.h"
#include "pc-identifiers.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "base/base64.h"

static FUNCTION_RETURN generate_default_pc_id(PcIdentifier * identifiers,
		unsigned int * num_identifiers) {
	size_t adapter_num, disk_num;
	FUNCTION_RETURN result_adapterInfos, result_diskinfos;
	unsigned int required_id_size, i, j, k;
	DiskInfo * diskInfos;
	AdapterInfo *adapterInfos;

	result_adapterInfos = getAdapterInfos(NULL, &adapter_num);
	if (result_adapterInfos != OK) {
		//call generate_disk_pc_id;
		return result_adapterInfos;
	}
	result_diskinfos = getDiskInfos(NULL, &disk_num);
	if (result_diskinfos == OK) {
		required_id_size = disk_num * adapter_num;
	} else {
		required_id_size = disk_num;
	}
	int defined_identifiers = *num_identifiers;
	*num_identifiers = required_id_size;
	if (identifiers == NULL) {
		return OK;
	} else if (required_id_size > defined_identifiers) {
		return BUFFER_TOO_SMALL;
	}
	diskInfos = (DiskInfo*) malloc(disk_num * sizeof(DiskInfo));
	result_diskinfos = getDiskInfos(diskInfos, &disk_num);
	adapterInfos = (AdapterInfo*) malloc(adapter_num * sizeof(AdapterInfo));
	result_adapterInfos = getAdapterInfos(adapterInfos, &adapter_num);
	for (i = 0; i < disk_num; i++) {
		for (j = 0; j < adapter_num; j++) {
			for (k = 0; k < 6; k++)
				identifiers[i * adapter_num + j][k] =
						diskInfos[i].disk_sn[k + 2]
								^ adapterInfos[j].mac_address[k + 2];
		}
	}

	free(diskInfos);
	free(adapterInfos);
	return OK;
}

static FUNCTION_RETURN generate_ethernet_pc_id(PcIdentifier * identifiers,
		unsigned int * num_identifiers, bool use_mac) {
	size_t adapters;
	FUNCTION_RETURN result_adapterInfos;
	unsigned int i, j, k;
	AdapterInfo *adapterInfos;

	result_adapterInfos = getAdapterInfos(NULL, &adapters);
	if (result_adapterInfos != OK) {
		return result_adapterInfos;
	}

	int defined_adapters = *num_identifiers;
	*num_identifiers = adapters;
	if (identifiers == NULL) {
		return OK;
	} else if (adapters > defined_adapters) {
		return BUFFER_TOO_SMALL;
	}

	adapterInfos = (AdapterInfo*) malloc(adapters * sizeof(AdapterInfo));
	result_adapterInfos = getAdapterInfos(adapterInfos, &adapters);
	for (j = 0; j < adapters; i++) {
		for (k = 0; k < 6; k++)
			if (use_mac) {
				identifiers[j][k] = adapterInfos[j].mac_address[k + 2];
			} else {
				//use ip
				if (k < 4) {
					identifiers[j][k] = adapterInfos[j].ipv4_address[k];
				} else {
					//padding
					identifiers[j][k] = 42;
				}
			}
	}
	free(adapterInfos);
	return OK;
}

static FUNCTION_RETURN generate_disk_pc_id(PcIdentifier * identifiers,
		unsigned int * num_identifiers, bool use_label) {
	size_t disk_num;
	FUNCTION_RETURN result_diskinfos;
	unsigned int i, k;
	DiskInfo * diskInfos;

	result_diskinfos = getDiskInfos(NULL, &disk_num);
	if (result_diskinfos != OK) {
		return result_diskinfos;
	}

	int defined_identifiers = *num_identifiers;
	*num_identifiers = disk_num;
	if (identifiers == NULL) {
		return OK;
	} else if (disk_num > defined_identifiers) {
		return BUFFER_TOO_SMALL;
	}

	diskInfos = (DiskInfo*) malloc(disk_num * sizeof(DiskInfo));
	result_diskinfos = getDiskInfos(diskInfos, &disk_num);

	for (i = 0; i < disk_num; i++) {
		for (k = 0; k < 6; k++) {
			if (use_label) {
				identifiers[i][k] = diskInfos[i].label[k];
			} else {
				identifiers[i][k] = diskInfos[i].disk_sn[k + 2];
			}
		}
	}
	free(diskInfos);
	return OK;
}

/**
 *
 * Calculates all the possible identifiers for the current machine, for the
 * given calculation strategy requested. Pc identifiers are more than one,
 * for instance a machine with more than one disk and one network interface has
 * usually multiple identifiers.
 *
 * First 4 bit of each pc identifier are reserved 3 for the type of strategy
 * used in calculation and 1 for parity checks (not implemented here)
 *
 * @param identifiers
 * @param array_size
 * @param
 * @return
 */
FUNCTION_RETURN generate_pc_id(PcIdentifier * identifiers,
		unsigned int * array_size, IDENTIFICATION_STRATEGY strategy) {
	FUNCTION_RETURN result;
	unsigned int i, j;
	const unsigned int original_array_size = *array_size;
	unsigned char strategy_num;
	switch (strategy) {
	case DEFAULT:
		result = generate_default_pc_id(identifiers, array_size);
		break;
	case ETHERNET:
		result = generate_ethernet_pc_id(identifiers, array_size, true);
		break;
	case IP_ADDRESS:
		result = generate_ethernet_pc_id(identifiers, array_size, false);
		break;
	case DISK_NUM:
		result = generate_disk_pc_id(identifiers, array_size, false);
		break;
	case DISK_LABEL:
		result = generate_disk_pc_id(identifiers, array_size, true);
		break;
	default:
		return ERROR;
	}

	if (result == OK && identifiers != NULL) {
		strategy_num = strategy << 5;
		for (i = 0; i < *array_size; i++) {
			//encode strategy in the first three bits of the pc_identifier
			identifiers[i][0] = (identifiers[i][0] & 15) | strategy_num;
		}
		//fill array if larger
		for (i = *array_size; i < original_array_size; i++) {
			identifiers[i][0] = STRATEGY_UNKNOWN;
			for (j = 1; j < sizeof(PcIdentifier); j++) {
				identifiers[i][j] = 42; //padding
			}
		}
	}
	return result;
}

char *MakeCRC(char *BitString) {
	static char Res[3];                                 // CRC Result
	char CRC[2];
	int i;
	char DoInvert;

	for (i = 0; i < 2; ++i)
		CRC[i] = 0;                    // Init before calculation

	for (i = 0; i < strlen(BitString); ++i) {
		DoInvert = ('1' == BitString[i]) ^ CRC[1];         // XOR required?

		CRC[1] = CRC[0];
		CRC[0] = DoInvert;
	}

	for (i = 0; i < 2; ++i)
		Res[1 - i] = CRC[i] ? '1' : '0'; // Convert binary to ASCII
	Res[2] = 0;                                         // Set string terminator

	return (Res);
}

FUNCTION_RETURN encode_pc_id(PcIdentifier identifier1, PcIdentifier identifier2,
		PcSignature pc_identifier_out) {
	//TODO base62 encoding, now uses base64
	PcIdentifier concat_identifiers[2];
	int b64_size = 0;
	size_t concatIdentifiersSize = sizeof(PcIdentifier) * 2;
	//concat_identifiers = (PcIdentifier *) malloc(concatIdentifiersSize);
	memcpy(&concat_identifiers[0], identifier1, sizeof(PcIdentifier));
	memcpy(&concat_identifiers[1], identifier2, sizeof(PcIdentifier));
	char* b64_data = base64(concat_identifiers, concatIdentifiersSize,
			&b64_size);
	if (b64_size > sizeof(PcSignature)) {
		return BUFFER_TOO_SMALL;
	}
	sprintf(pc_identifier_out, "%.4s-%.4s-%.4s-%.4s", &b64_data[0],
			&b64_data[4], &b64_data[8], &b64_data[12]);
	//free(concat_identifiers);
	free(b64_data);
	return OK;
}

FUNCTION_RETURN parity_check_id(PcSignature pc_identifier) {
	return OK;
}

FUNCTION_RETURN generate_user_pc_signature(PcSignature identifier_out,
		IDENTIFICATION_STRATEGY strategy) {
	FUNCTION_RETURN result;
	PcIdentifier* identifiers;
	unsigned int req_buffer_size = 0;
	result = generate_pc_id(NULL, &req_buffer_size, strategy);
	if (result != OK) {
		return result;
	}
	if (req_buffer_size == 0) {
		return ERROR;
	}
	req_buffer_size = req_buffer_size < 2 ? 2 : req_buffer_size;
	identifiers = (PcIdentifier *) malloc(
			sizeof(PcIdentifier) * req_buffer_size);
	result = generate_pc_id(identifiers, &req_buffer_size, strategy);
	if (result != OK) {
		free(identifiers);
		return result;
	}
	result = encode_pc_id(identifiers[0], identifiers[1], identifier_out);
	free(identifiers);
	return result;
}

/**
 * Extract the two pc identifiers from the user provided code.
 * @param identifier1_out
 * @param identifier2_out
 * @param str_code: the code in the string format XXXX-XXXX-XXXX-XXXX
 * @return
 */
static FUNCTION_RETURN decode_pc_id(PcIdentifier identifier1_out,
		PcIdentifier identifier2_out, PcSignature pc_signature_in) {
	//TODO base62 encoding, now uses base64

	unsigned char * concat_identifiers;
	char base64ids[17];
	int identifiers_size;

	sscanf(pc_signature_in, "%4s-%4s-%4s-%4s", &base64ids[0], &base64ids[4],
			&base64ids[8], &base64ids[12]);
	concat_identifiers = unbase64(base64ids, 16, &identifiers_size);
	if (identifiers_size > sizeof(PcIdentifier) * 2) {
		return BUFFER_TOO_SMALL;
	}
	memcpy(identifier1_out, concat_identifiers, sizeof(PcIdentifier));
	memcpy(identifier2_out, concat_identifiers + sizeof(PcIdentifier),
			sizeof(PcIdentifier));
	free(concat_identifiers);
	return OK;
}

static IDENTIFICATION_STRATEGY strategy_from_pc_id(PcIdentifier identifier) {
	return (IDENTIFICATION_STRATEGY) identifier[0] >> 5;
}

EVENT_TYPE validate_pc_signature(PcSignature str_code) {
	PcIdentifier user_identifiers[2];
	FUNCTION_RETURN result;
	IDENTIFICATION_STRATEGY previous_strategy_id, current_strategy_id;
	PcIdentifier* calculated_identifiers = NULL;
	unsigned int calc_identifiers_size = 0;
	int i = 0, j = 0;
	//bool found;

	result = decode_pc_id(user_identifiers[0], user_identifiers[1], str_code);
	if (result != OK) {
		return result;
	}
	previous_strategy_id = STRATEGY_UNKNOWN;
	//found = false;
	for (i = 0; i < 2; i++) {
		current_strategy_id = strategy_from_pc_id(user_identifiers[i]);
		if (current_strategy_id == STRATEGY_UNKNOWN) {
			return LICENSE_MALFORMED;
		}
		if (current_strategy_id != previous_strategy_id) {
			if (calculated_identifiers != NULL) {
				free(calculated_identifiers);
			}
			current_strategy_id = previous_strategy_id;
			generate_pc_id(NULL, &calc_identifiers_size, current_strategy_id);
			calculated_identifiers = (PcIdentifier *) malloc(
					sizeof(PcIdentifier) * calc_identifiers_size);
			generate_pc_id(calculated_identifiers, &calc_identifiers_size,
					current_strategy_id);
		}
		//maybe skip the byte 0
		for (j = 0; j < calc_identifiers_size; j++) {
			if (!memcmp(user_identifiers[i], calculated_identifiers[j],
					sizeof(PcIdentifier))) {
				free(calculated_identifiers);
				return LICENSE_OK;
			}
		}
	}
	free(calculated_identifiers);
	return IDENTIFIERS_MISMATCH;
}
