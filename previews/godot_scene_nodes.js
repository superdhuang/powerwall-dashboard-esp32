export function* sceneNodes(text) {
  // Splitting at headers preserves empty nodes: their children must not
  // supply properties or transforms to them.
  for (const block of text.split(/(?=^\[node )/m)) {
    const match = block.match(/^\[node ([^\]]+)\]/);
    if (match) yield { header: match[1], properties: block.slice(match[0].length) };
  }
}
