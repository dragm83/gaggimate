import { useState, useRef, useEffect, useCallback } from 'preact/hooks';
import { createPortal } from 'preact/compat';
import { computePosition, flip, shift, offset, autoUpdate } from '@floating-ui/dom';

/**
 * Tooltip component that renders in a portal with automatic positioning.
 * Uses Floating UI for edge detection and repositioning.
 *
 * @param {Object} props
 * @param {string} props.content - Tooltip text content
 * @param {preact.ComponentChildren} props.children - Trigger element
 * @param {'top'|'bottom'|'left'|'right'} [props.placement='top'] - Preferred placement
 */
export function Tooltip({ content, children, placement = 'top', disabled = false }) {
  const [isVisible, setIsVisible] = useState(false);
  const [position, setPosition] = useState({ x: 0, y: 0 });
  const [actualPlacement, setActualPlacement] = useState(placement);
  const triggerRef = useRef(null);
  const tooltipRef = useRef(null);

  useEffect(() => {
    if (disabled && isVisible) {
      setIsVisible(false);
    }
  }, [disabled, isVisible]);

  useEffect(() => {
    if (!isVisible || !triggerRef.current || !tooltipRef.current || disabled) return;

    // Snapshot both elements for the lifetime of this effect. The refs are
    // cleared as soon as a profile-list refresh unmounts the trigger/portal,
    // while Floating UI may still have a queued observer callback. Reading
    // ref.current inside that callback can therefore pass null to
    // getComputedStyle().
    const triggerElement = triggerRef.current;
    const tooltipElement = tooltipRef.current;
    let cancelled = false;

    const cleanup = autoUpdate(triggerElement, tooltipElement, () => {
      computePosition(triggerElement, tooltipElement, {
        placement,
        strategy: 'fixed',
        middleware: [
          offset(8), // 8px gap from trigger
          flip(), // Flip to opposite side if no space
          shift({ padding: 8 }), // Shift along axis to stay in viewport
        ],
      }).then(({ x, y, placement: finalPlacement }) => {
        if (cancelled) return;
        setPosition(pos => {
          // Only update if changed to prevent render loops
          if (pos.x === x && pos.y === y) return pos;
          return { x, y };
        });
        setActualPlacement(finalPlacement);
      });
    });

    return () => {
      cancelled = true;
      cleanup();
    };
  }, [disabled, isVisible, placement]);

  const show = useCallback(() => {
    if (!disabled) setIsVisible(true);
  }, [disabled]);
  const hide = useCallback(() => setIsVisible(false), []);

  const tooltip =
    isVisible &&
    createPortal(
      <div
        ref={tooltipRef}
        role='tooltip'
        className='tooltip-portal'
        data-placement={actualPlacement}
        style={{
          position: 'fixed',
          left: `${position.x}px`,
          top: `${position.y}px`,
        }}
      >
        {content}
      </div>,
      document.body,
    );

  return (
    <>
      <span
        ref={triggerRef}
        onMouseEnter={show}
        onMouseLeave={hide}
        onFocus={show}
        onBlur={hide}
        className='inline-flex'
      >
        {children}
      </span>
      {tooltip}
    </>
  );
}
