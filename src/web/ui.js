window.FizixUI = (() => {
  const defaults = {
    shapeType: 2,
    positionX: 500,
    positionY: 500,
    velocityX: 0,
    velocityY: 0,
    boxWidth: 60,
    boxHeight: 60,
    base: 300,
    angle: 30,
    radius: 30,
    mass: 10,
    staticFriction: 1,
    kineticFriction: 0.7,
    flip: true,
    color: "#ffffff",
    alpha: 1,
    restitution: 0.4,
    bodyType: 1,
    dragMode: 1,
  };

  const state = {
    module: null,
  };

  const elements = {};

  const cache = (id) => {
    elements[id] = document.getElementById(id);
    return elements[id];
  };

  const toNumber = (value, fallback) => {
    const parsed = Number.parseFloat(value);
    return Number.isFinite(parsed) ? parsed : fallback;
  };

  const toInt = (value, fallback) => {
    const parsed = Number.parseInt(value, 10);
    return Number.isFinite(parsed) ? parsed : fallback;
  };

  const hexToRgb = (hex) => {
    const normalized = hex.replace("#", "").trim();
    if (normalized.length !== 6) {
      return { r: 255, g: 255, b: 255 };
    }
    return {
      r: Number.parseInt(normalized.slice(0, 2), 16),
      g: Number.parseInt(normalized.slice(2, 4), 16),
      b: Number.parseInt(normalized.slice(4, 6), 16),
    };
  };

  const callNative = (name, returnType, argTypes, args) => {
    if (!state.module || !state.module.ccall) {
      return;
    }
    state.module.ccall(name, returnType, argTypes, args);
  };

  const updateVisibility = () => {
    const shapeType = toInt(elements.shapeType.value, defaults.shapeType);
    const showBox = shapeType === 2;
    const showBall = shapeType === 0;
    const showIncline = shapeType === 1;
    const showCanon = shapeType === 3;

    elements.boxControls.hidden = !showBox;
    elements.ballControls.hidden = !showBall;
    elements.inclineControls.hidden = !showIncline;
    elements.canonControls.hidden = !showCanon;
    elements.rigidbodyControls.hidden = !(showBox || showBall);
    elements.frictionControls.hidden = !showIncline;
  };

  const readSpawnSettings = () => {
    const { r, g, b } = hexToRgb(elements.color.value);
    const shapeType = toInt(elements.shapeType.value, defaults.shapeType);
    const useInclineAngle = shapeType === 1;
    return {
      shapeType,
      positionX: toNumber(elements.positionX.value, defaults.positionX),
      positionY: toNumber(elements.positionY.value, defaults.positionY),
      velocityX: toNumber(elements.velocityX.value, defaults.velocityX),
      velocityY: toNumber(elements.velocityY.value, defaults.velocityY),
      boxWidth: toNumber(elements.boxWidth.value, defaults.boxWidth),
      boxHeight: toNumber(elements.boxHeight.value, defaults.boxHeight),
      base: toNumber(elements.base.value, defaults.base),
      angle: useInclineAngle ? toNumber(elements.inclineAngle.value, defaults.angle) : toNumber(elements.canonAngle.value, defaults.angle),
      radius: toNumber(elements.radius.value, defaults.radius),
      mass: toNumber(elements.mass.value, defaults.mass),
      staticFriction: toNumber(elements.staticFriction.value, defaults.staticFriction),
      kineticFriction: toNumber(elements.kineticFriction.value, defaults.kineticFriction),
      flip: elements.flip.checked,
      colorR: r,
      colorG: g,
      colorB: b,
      colorA: toNumber(elements.alpha.value, defaults.alpha),
      restitution: toNumber(elements.restitution.value, defaults.restitution),
      bodyType: toInt(elements.bodyType.value, defaults.bodyType),
    };
  };

  const sendSpawn = () => {
    const settings = readSpawnSettings();
    callNative(
      "UIManager_RequestSpawn",
      null,
      [
        "number",
        "number",
        "number",
        "number",
        "number",
        "number",
        "number",
        "number",
        "number",
        "number",
        "number",
        "number",
        "number",
        "number",
        "number",
        "number",
        "number",
        "number",
        "number",
        "number",
      ],
      [
        settings.shapeType,
        settings.positionX,
        settings.positionY,
        settings.velocityX,
        settings.velocityY,
        settings.boxWidth,
        settings.boxHeight,
        settings.base,
        settings.angle,
        settings.radius,
        settings.mass,
        settings.staticFriction,
        settings.kineticFriction,
        settings.flip ? 1 : 0,
        settings.colorR,
        settings.colorG,
        settings.colorB,
        settings.colorA,
        settings.restitution,
        settings.bodyType,
      ]
    );
  };

  const initialize = (module) => {
    state.module = module;

    cache("bodyCount");
    cache("dragMode");
    cache("clearBodies");
    cache("shapeType");
    cache("positionX");
    cache("positionY");
    cache("velocityX");
    cache("velocityY");
    cache("boxWidth");
    cache("boxHeight");
    cache("base");
    cache("inclineAngle");
    cache("canonAngle");
    cache("radius");
    cache("mass");
    cache("staticFriction");
    cache("kineticFriction");
    cache("frictionControls");
    cache("flip");
    cache("color");
    cache("alpha");
    cache("restitution");
    cache("bodyType");
    cache("spawnObject");
    cache("boxControls");
    cache("ballControls");
    cache("inclineControls");
    cache("canonControls");
    cache("rigidbodyControls");

    elements.dragMode.value = String(defaults.dragMode);
    elements.shapeType.value = String(defaults.shapeType);
    elements.positionX.value = defaults.positionX;
    elements.positionY.value = defaults.positionY;
    elements.velocityX.value = defaults.velocityX;
    elements.velocityY.value = defaults.velocityY;
    elements.boxWidth.value = defaults.boxWidth;
    elements.boxHeight.value = defaults.boxHeight;
    elements.base.value = defaults.base;
    elements.inclineAngle.value = defaults.angle;
    elements.canonAngle.value = defaults.angle;
    elements.radius.value = defaults.radius;
    elements.mass.value = defaults.mass;
    elements.staticFriction.value = defaults.staticFriction;
    elements.kineticFriction.value = defaults.kineticFriction;
    elements.flip.checked = defaults.flip;
    elements.color.value = defaults.color;
    elements.alpha.value = defaults.alpha;
    elements.restitution.value = defaults.restitution;
    elements.bodyType.value = String(defaults.bodyType);

    elements.dragMode.addEventListener("change", () => {
      callNative("UIManager_SetDragMode", null, ["number"], [toInt(elements.dragMode.value, defaults.dragMode)]);
    });

    elements.clearBodies.addEventListener("click", () => {
      callNative("UIManager_ClearBodies", null, [], []);
    });

    elements.shapeType.addEventListener("change", updateVisibility);
    elements.spawnObject.addEventListener("click", sendSpawn);

    updateVisibility();
    callNative("UIManager_SetDragMode", null, ["number"], [defaults.dragMode]);
  };

  const setStats = (bodyCount) => {
    if (elements.bodyCount) {
      elements.bodyCount.textContent = String(bodyCount);
    }
  };

  return {
    initialize,
    setStats,
  };
})();
