import { useEffect, useRef, useState } from 'react';
import * as THREE from 'three';
import { attitudeToEuler, dampAngle } from '../core/attitude';
import type { Attitude } from '../core/types';
import { clamp, formatDegrees } from '../core/types';

interface DroneAttitudeViewProps {
  attitude: Attitude;
  motors: [number, number, number, number];
  stale: boolean;
}

interface DroneScene {
  root: THREE.Group;
  propellers: THREE.Mesh[];
}

const MOTOR_POSITIONS = [
  { x: 0.88, z: -0.88 },
  { x: 0.88, z: 0.88 },
  { x: -0.88, z: 0.88 },
  { x: -0.88, z: -0.88 },
];

export function DroneAttitudeView({ attitude, motors, stale }: DroneAttitudeViewProps) {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const attitudeRef = useRef(attitude);
  const motorsRef = useRef(motors);
  const [webglUnavailable, setWebglUnavailable] = useState(false);

  useEffect(() => {
    attitudeRef.current = attitude;
  }, [attitude]);

  useEffect(() => {
    motorsRef.current = motors;
  }, [motors]);

  useEffect(() => {
    const container = containerRef.current;
    if (!container) return undefined;

    let renderer: THREE.WebGLRenderer;
    try {
      renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true, powerPreference: 'high-performance' });
    } catch {
      setWebglUnavailable(true);
      return undefined;
    }

    setWebglUnavailable(false);
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer.outputColorSpace = THREE.SRGBColorSpace;
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 0.9;
    renderer.domElement.setAttribute('aria-label', '3D UltraHawk vehicle orientation');
    renderer.domElement.setAttribute('role', 'img');
    container.replaceChildren(renderer.domElement);

    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(32, 1, 0.1, 100);
    const drone = buildDroneScene();
    scene.add(drone.root);

    const ambient = new THREE.HemisphereLight(0xe2e8e6, 0x1a2022, 1.8);
    const keyLight = new THREE.DirectionalLight(0xffffff, 2.4);
    keyLight.position.set(3, 5, 4);
    scene.add(ambient, keyLight);

    const grid = new THREE.GridHelper(6, 12, 0x3c474a, 0x252d30);
    grid.position.y = -0.32;
    scene.add(grid);

    const ground = new THREE.Mesh(
      new THREE.CircleGeometry(2.8, 48),
      new THREE.MeshBasicMaterial({ color: 0x111719, transparent: true, opacity: 0.72 }),
    );
    ground.rotation.x = -Math.PI / 2;
    ground.position.y = -0.31;
    scene.add(ground);

    const orbit = { azimuth: 0.72, elevation: 0.5, distance: 4.8 };
    let animationFrame = 0;
    let lastTime = performance.now();
    let dragging = false;
    let pointerX = 0;
    let pointerY = 0;

    const updateCamera = () => {
      const horizontal = orbit.distance * Math.cos(orbit.elevation);
      camera.position.set(
        horizontal * Math.sin(orbit.azimuth),
        orbit.distance * Math.sin(orbit.elevation),
        horizontal * Math.cos(orbit.azimuth),
      );
      camera.lookAt(0, 0.05, 0);
    };

    const resize = () => {
      const width = Math.max(1, container.clientWidth);
      const height = Math.max(1, container.clientHeight);
      camera.aspect = width / height;
      camera.updateProjectionMatrix();
      renderer.setSize(width, height, false);
    };

    const onPointerDown = (event: PointerEvent) => {
      dragging = true;
      pointerX = event.clientX;
      pointerY = event.clientY;
      container.setPointerCapture(event.pointerId);
    };
    const onPointerMove = (event: PointerEvent) => {
      if (!dragging) return;
      orbit.azimuth -= (event.clientX - pointerX) * 0.008;
      orbit.elevation = clamp(orbit.elevation + (event.clientY - pointerY) * 0.008, 0.12, 1.25);
      pointerX = event.clientX;
      pointerY = event.clientY;
      updateCamera();
    };
    const onPointerUp = (event: PointerEvent) => {
      dragging = false;
      if (container.hasPointerCapture(event.pointerId)) container.releasePointerCapture(event.pointerId);
    };
    const onWheel = (event: WheelEvent) => {
      event.preventDefault();
      orbit.distance = clamp(orbit.distance + event.deltaY * 0.004, 3.1, 7.2);
      updateCamera();
    };
    const onDoubleClick = () => {
      orbit.azimuth = 0.72;
      orbit.elevation = 0.5;
       orbit.distance = 4.8;
      updateCamera();
    };

    container.addEventListener('pointerdown', onPointerDown);
    container.addEventListener('pointermove', onPointerMove);
    container.addEventListener('pointerup', onPointerUp);
    container.addEventListener('pointercancel', onPointerUp);
    container.addEventListener('wheel', onWheel, { passive: false });
    container.addEventListener('dblclick', onDoubleClick);
    const resizeObserver = new ResizeObserver(resize);
    resizeObserver.observe(container);
    updateCamera();
    resize();

    const render = (time: number) => {
      const delta = Math.min(0.05, (time - lastTime) / 1000);
      lastTime = time;
      const target = attitudeToEuler(attitudeRef.current);
      drone.root.rotation.order = 'YXZ';
      drone.root.rotation.x = dampAngle(drone.root.rotation.x, target.x, 9, delta);
      drone.root.rotation.y = dampAngle(drone.root.rotation.y, target.y, 9, delta);
      drone.root.rotation.z = dampAngle(drone.root.rotation.z, target.z, 9, delta);

      drone.propellers.forEach((propeller, index) => {
        const output = clamp(Math.abs(motorsRef.current[index] ?? 0), 0, 1);
        propeller.rotation.y += delta * (0.25 + output * 10);
        propeller.scale.setScalar(0.72 + output * 0.28);
      });

      renderer.render(scene, camera);
      animationFrame = window.requestAnimationFrame(render);
    };
    animationFrame = window.requestAnimationFrame(render);

    return () => {
      window.cancelAnimationFrame(animationFrame);
      resizeObserver.disconnect();
      container.removeEventListener('pointerdown', onPointerDown);
      container.removeEventListener('pointermove', onPointerMove);
      container.removeEventListener('pointerup', onPointerUp);
      container.removeEventListener('pointercancel', onPointerUp);
      container.removeEventListener('wheel', onWheel);
      container.removeEventListener('dblclick', onDoubleClick);
      disposeObject(scene);
      renderer.dispose();
      container.replaceChildren();
    };
  }, []);

  if (webglUnavailable) {
    return <div className="drone-fallback"><strong>3D view unavailable</strong><span>WebGL is not available in this browser. The flight instrument remains active.</span></div>;
  }

  return (
    <div className={`drone-view ${stale ? 'is-stale' : ''}`}>
      <div ref={containerRef} className="drone-canvas" />
       <div className="drone-overlay drone-overlay-top"><span>Interactive view</span><span>Drag to rotate</span></div>
      <div className="drone-overlay drone-overlay-bottom">
         <span className="drone-axis"><i className="axis-forward" /> Nose</span>
        <span>R {formatDegrees(attitude.roll)} / P {formatDegrees(attitude.pitch)} / Y {formatDegrees(attitude.yaw)}</span>
      </div>
      {stale && <div className="drone-stale">TELEMETRY STALE</div>}
    </div>
  );
}

function buildDroneScene(): DroneScene {
  const root = new THREE.Group();
  const bodyMaterial = new THREE.MeshStandardMaterial({ color: 0x4a5355, roughness: 0.76, metalness: 0.16 });
  const darkMaterial = new THREE.MeshStandardMaterial({ color: 0x242a2c, roughness: 0.82, metalness: 0.08 });
  const accentMaterial = new THREE.MeshStandardMaterial({ color: 0xc5a15a, roughness: 0.64, metalness: 0.12 });
  const rotorMaterial = new THREE.MeshStandardMaterial({ color: 0xb8c4c2, transparent: true, opacity: 0.18, roughness: 0.45 });
  const propellers: THREE.Mesh[] = [];

  const body = new THREE.Mesh(new THREE.BoxGeometry(1.05, 0.25, 0.72), bodyMaterial);
  body.position.y = 0.04;
  root.add(body);

  const top = new THREE.Mesh(new THREE.CylinderGeometry(0.33, 0.42, 0.18, 24), darkMaterial);
  top.position.y = 0.24;
  root.add(top);

  const armGeometry = new THREE.BoxGeometry(2.2, 0.11, 0.12);
  const armA = new THREE.Mesh(armGeometry, darkMaterial);
  armA.rotation.y = Math.PI / 4;
  const armB = new THREE.Mesh(armGeometry, darkMaterial);
  armB.rotation.y = -Math.PI / 4;
  armA.position.y = 0.02;
  armB.position.y = 0.02;
  root.add(armA, armB);

  const nose = new THREE.Mesh(new THREE.ConeGeometry(0.13, 0.3, 4), accentMaterial);
  nose.rotation.x = -Math.PI / 2;
  nose.position.set(0, 0.2, -0.55);
  root.add(nose);

  MOTOR_POSITIONS.forEach(({ x, z }) => {
    const motor = new THREE.Mesh(new THREE.CylinderGeometry(0.19, 0.19, 0.16, 18), bodyMaterial);
    motor.position.set(x, 0.08, z);
    root.add(motor);

    const propeller = new THREE.Mesh(new THREE.CylinderGeometry(0.3, 0.3, 0.018, 32), rotorMaterial.clone());
    propeller.position.set(x, 0.19, z);
    root.add(propeller);
    propellers.push(propeller);
  });

  return { root, propellers };
}

function disposeObject(object: THREE.Object3D): void {
  object.traverse((child) => {
    if (!(child instanceof THREE.Mesh)) return;
    child.geometry.dispose();
    const materials = Array.isArray(child.material) ? child.material : [child.material];
    materials.forEach((material) => material.dispose());
  });
}
