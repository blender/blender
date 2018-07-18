/*
 * Copyright 2018, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor:
 *              Lukas Stockner
 *              Stefan Werner
 */

#include "COM_CryptomatteNode.h"
#include "COM_CryptomatteOperation.h"
#include "COM_SetAlphaOperation.h"
#include "COM_ConvertOperation.h"
#include "BLI_string.h"
#include "BLI_hash_mm3.h"
#include "BLI_assert.h"
#include <iterator>

CryptomatteNode::CryptomatteNode(bNode *editorNode) : Node(editorNode)
{
	/* pass */
}

/* This is taken from the Cryptomatte specification 1.0. */
static inline float hash_to_float(uint32_t hash)
{
	uint32_t mantissa = hash & (( 1 << 23) - 1);
	uint32_t exponent = (hash >> 23) & ((1 << 8) - 1);
	exponent = max(exponent, (uint32_t) 1);
	exponent = min(exponent, (uint32_t) 254);
	exponent = exponent << 23;
	uint32_t sign = (hash >> 31);
	sign = sign << 31;
	uint32_t float_bits = sign | exponent | mantissa;
	float f;
	/* Bit casting relies on equal size for both types. */
	BLI_STATIC_ASSERT(sizeof(float) == sizeof(uint32_t), "float and uint32_t are not the same size")
	::memcpy(&f, &float_bits, sizeof(float));
	return f;
}

void CryptomatteNode::convertToOperations(NodeConverter &converter, const CompositorContext &/*context*/) const
{
	NodeInput *inputSocketImage = this->getInputSocket(0);
	NodeOutput *outputSocketImage = this->getOutputSocket(0);
	NodeOutput *outputSocketMatte = this->getOutputSocket(1);
	NodeOutput *outputSocketPick = this->getOutputSocket(2);

	bNode *node = this->getbNode();
	NodeCryptomatte *cryptoMatteSettings = (NodeCryptomatte *)node->storage;

	CryptomatteOperation *operation = new CryptomatteOperation(getNumberOfInputSockets()-1);
	if (cryptoMatteSettings) {
		if (cryptoMatteSettings->matte_id) {
			/* Split the string by commas, ignoring white space. */
			std::string input = cryptoMatteSettings->matte_id;
			std::istringstream ss(input);
			while (ss.good()) {
				std::string token;
				getline(ss, token, ',');
				/* Ignore empty tokens. */
				if (token.length() > 0) {
					size_t first = token.find_first_not_of(' ');
					size_t last = token.find_last_not_of(' ');
					if (first == std::string::npos || last == std::string::npos) {
						break;
					}
					token = token.substr(first, (last - first + 1));
					if (*token.begin() == '<' && *(--token.end()) == '>') {
						operation->addObjectIndex(atof(token.substr(1, token.length() - 2).c_str()));
					}
					else {
						uint32_t hash = BLI_hash_mm3((const unsigned char*)token.c_str(), token.length(), 0);
						operation->addObjectIndex(hash_to_float(hash));
					}
				}
			}
		}
	}

	converter.addOperation(operation);

	for (int i = 0; i < getNumberOfInputSockets()-1; ++i) {
		converter.mapInputSocket(this->getInputSocket(i + 1), operation->getInputSocket(i));
	}

	SeparateChannelOperation *separateOperation = new SeparateChannelOperation;
	separateOperation->setChannel(3);
	converter.addOperation(separateOperation);
	
	SetAlphaOperation *operationAlpha = new SetAlphaOperation();
	converter.addOperation(operationAlpha);

	converter.addLink(operation->getOutputSocket(0), separateOperation->getInputSocket(0));
	converter.addLink(separateOperation->getOutputSocket(0), operationAlpha->getInputSocket(1));

	SetAlphaOperation *clearAlphaOperation = new SetAlphaOperation();
	converter.addOperation(clearAlphaOperation);
	converter.addInputValue(clearAlphaOperation->getInputSocket(1), 1.0f);

	converter.addLink(operation->getOutputSocket(0), clearAlphaOperation->getInputSocket(0));

	converter.mapInputSocket(inputSocketImage, operationAlpha->getInputSocket(0));
	converter.mapOutputSocket(outputSocketMatte, separateOperation->getOutputSocket(0));
	converter.mapOutputSocket(outputSocketImage, operationAlpha->getOutputSocket(0));
	converter.mapOutputSocket(outputSocketPick, clearAlphaOperation->getOutputSocket(0));
	
}
