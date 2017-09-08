#pragma once

#include <vector>
#include "Layer.h"

namespace canvas {

	class History {
	private:
		int index;
		std::vector<Layer> history;

	public:
		History();

		void push(Layer layers);
		Layer undo();
		Layer redo();
	};

}