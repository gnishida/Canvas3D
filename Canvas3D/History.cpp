#include "History.h"

namespace canvas {

	History::History() {
		index = -1;
	}

	void History::push(Layer layer) {
		// remove the index-th element and their after
		history.resize(index + 1);

		// add history
		Layer copied_layer = layer.clone();
		history.push_back(copied_layer);
		index++;
	}

	Layer History::undo() {
		if (index <= 0) throw "No history.";

		// return the previous state
		index--;
		Layer copied_layer = history[index].clone();
		return copied_layer;
	}

	Layer History::redo() {
		if (index >= history.size() - 1) throw "No history.";

		index++;
		Layer copied_layer = history[index].clone();
		return copied_layer;
	}

}